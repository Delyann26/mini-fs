#include "directory.h"
#include "fs_alloc.h"
#include "inode_table.h"
#include <math.h>

#include <string.h>

int directory_ensure_data_block(Disk *disk, Inode *inode, size_t logical_block_index, uint32_t *physical_block, bool *created_data_block,
                                bool *created_indirect_block) {
    if (disk == NULL || inode == NULL || physical_block == NULL || created_data_block == NULL || created_indirect_block == NULL) {
        return FS_ERR_NULL;
    }
    *created_data_block = false;
    *created_indirect_block = false;
    size_t maximum_blocks_supported = INODE_DIRECT_POINTERS_COUNT + INDIRECT_POINTERS_PER_BLOCK;
    if (logical_block_index >= maximum_blocks_supported) {
        return FS_ERR_DIRECTORY_IS_FULL;
    }

    int res;
    if (logical_block_index < INODE_DIRECT_POINTERS_COUNT) { // DIRECT
        uint32_t block = inode->direct_blocks[logical_block_index];
        if (block == INVALID_BLOCK) { // unassigned
            res = fs_allocate_data_block(disk, &block);
            if (res != FS_OK) {
                return res;
            }
            *created_data_block = true;
            uint8_t empty_block[BLOCK_SIZE];
            memset(empty_block, 0, sizeof(empty_block));
            res = disk_write(disk, empty_block, block);
            if (res != FS_OK) {
                fs_free_data_block(disk, block);
                *created_data_block = false;
                return res;
            }
            inode->direct_blocks[logical_block_index] = block;
        }
        if (block < USER_DATA_BLOCK_START || block >= MAXIMUM_BLOCKS) {
            return FS_ERR_INVALID_BLOCK;
        }
        *physical_block = block;
        return FS_OK;
    }

    // INDIRECT
    size_t indirect_index = logical_block_index - INODE_DIRECT_POINTERS_COUNT;
    uint32_t indirect_pointers[INDIRECT_POINTERS_PER_BLOCK];

    if (inode->indirect_block == INVALID_BLOCK) {
        uint32_t indirect_block;
        res = fs_allocate_data_block(disk, &indirect_block);
        if (res != FS_OK) {
            return res;
        }

        inode->indirect_block = indirect_block;
        *created_indirect_block = true;
        memset(indirect_pointers, 0, sizeof(indirect_pointers));
    } else {
        if (inode->indirect_block < USER_DATA_BLOCK_START || inode->indirect_block >= MAXIMUM_BLOCKS) {
            return FS_ERR_INVALID_BLOCK;
        }

        res = disk_read(disk, indirect_pointers, inode->indirect_block);
        if (res != FS_OK) {
            return res;
        }
    }

    uint32_t block = indirect_pointers[indirect_index];
    if (block == INVALID_BLOCK) {
        res = fs_allocate_data_block(disk, &block);
        if (res != FS_OK) {
            if (*created_indirect_block) {
                fs_free_data_block(disk, inode->indirect_block);
                inode->indirect_block = INVALID_BLOCK;
                *created_indirect_block = false;
            }
            return res;
        }

        *created_data_block = true;
        uint8_t empty_block[BLOCK_SIZE];
        memset(empty_block, 0, sizeof(empty_block));
        res = disk_write(disk, empty_block, block);

        if (res != FS_OK) {
            fs_free_data_block(disk, block);
            *created_data_block = false;
            if (*created_indirect_block) {
                fs_free_data_block(disk, inode->indirect_block);
                inode->indirect_block = INVALID_BLOCK;
                *created_indirect_block = false;
            }
            return res;
        }

        indirect_pointers[indirect_index] = block;
        res = disk_write(disk, indirect_pointers, inode->indirect_block);

        if (res != FS_OK) {
            fs_free_data_block(disk, block);
            *created_data_block = false;

            if (*created_indirect_block) {
                fs_free_data_block(disk, inode->indirect_block);
                inode->indirect_block = INVALID_BLOCK;
                *created_indirect_block = false;
            }

            return res;
        }
    }

    if (block < USER_DATA_BLOCK_START || block >= MAXIMUM_BLOCKS) {
        return FS_ERR_INVALID_BLOCK;
    }
    *physical_block = block;
    return FS_OK;
}

int directory_rollback_data_block(Disk *disk, Inode *inode, size_t logical_block_index, uint32_t physical_block, bool created_data_block,
                                  bool created_indirect_block) {
    if (!created_data_block) {
        return FS_OK;
    }
    // DIRECT
    if (logical_block_index < INODE_DIRECT_POINTERS_COUNT) {
        inode->direct_blocks[logical_block_index] = INVALID_BLOCK;
        return fs_free_data_block(disk, physical_block);
    }
    // INDIRECT
    if (created_indirect_block) {
        uint32_t indirect_block = inode->indirect_block;
        inode->indirect_block = INVALID_BLOCK;
        int data_res = fs_free_data_block(disk, physical_block);
        int indirect_res = fs_free_data_block(disk, indirect_block);
        if (data_res != FS_OK) {
            return data_res;
        }
        return indirect_res;
    }

    uint32_t indirect_pointers[INDIRECT_POINTERS_PER_BLOCK];
    int res = disk_read(disk, indirect_pointers, inode->indirect_block);
    if (res != FS_OK) {
        return res;
    }
    size_t indirect_index = logical_block_index - INODE_DIRECT_POINTERS_COUNT;
    indirect_pointers[indirect_index] = INVALID_BLOCK;
    res = disk_write(disk, indirect_pointers, inode->indirect_block);
    if (res != FS_OK) {
        return res;
    }
    return fs_free_data_block(disk, physical_block);
}

static int directory_find_entry_internal(Disk *disk, const Inode *dir_inode, const char *name, uint32_t *inode_number,
                                         size_t *logical_directory_entry, uint32_t *physical_block, size_t *entry_index) {

    if (disk == NULL || dir_inode == NULL || name == NULL) {
        return FS_ERR_NULL;
    }
    size_t name_length = strlen(name);
    if (name_length == 0 || name_length > MAX_FILENAME_LENGTH) {
        return FS_ERR_INVALID_FILE_NAME;
    }
    if (dir_inode->type != INODE_TYPE_DIRECTORY) {
        return FS_ERR_ENTRY_IS_NOT_DIRECTORY;
    }
    if (dir_inode->size % DIRECTORY_ENTRY_SIZE != 0) {
        return FS_ERR_INVALID_DISK;
    }
    size_t entries_count = dir_inode->size / DIRECTORY_ENTRY_SIZE;
    if (entries_count == 0) {
        return FS_ERR_ENTRY_NOT_FOUND;
    }
    size_t blocks_needed = (size_t)ceil((double)entries_count / DIRECTORY_ENTRIES_PER_BLOCK);
    size_t maximum_blocks_needed = INODE_DIRECT_POINTERS_COUNT + INDIRECT_POINTERS_PER_BLOCK;
    if (blocks_needed > maximum_blocks_needed) {
        return FS_ERR_INVALID_DISK;
    }

    uint32_t indirect_pointers[INDIRECT_POINTERS_PER_BLOCK];
    int res;
    if (blocks_needed > INODE_DIRECT_POINTERS_COUNT) {
        if (dir_inode->indirect_block < USER_DATA_BLOCK_START || dir_inode->indirect_block >= MAXIMUM_BLOCKS) {
            return FS_ERR_INVALID_BLOCK;
        }
        res = disk_read(disk, indirect_pointers, dir_inode->indirect_block);
        if (res != FS_OK) {
            return res;
        }
    }

    size_t entries_processed = 0;

    for (size_t block_index = 0; block_index < blocks_needed; block_index++) {
        uint32_t block_to_process;
        if (block_index < INODE_DIRECT_POINTERS_COUNT) {
            block_to_process = dir_inode->direct_blocks[block_index];
        } else {
            size_t indirect_index = block_index - INODE_DIRECT_POINTERS_COUNT;
            block_to_process = indirect_pointers[indirect_index];
        }

        if (block_to_process < USER_DATA_BLOCK_START || block_to_process >= MAXIMUM_BLOCKS) {
            return FS_ERR_INVALID_BLOCK;
        }

        DirectoryEntry entries[DIRECTORY_ENTRIES_PER_BLOCK];
        res = disk_read(disk, entries, block_to_process);
        if (res != FS_OK) {
            return res;
        }

        size_t entries_remaining = entries_count - entries_processed;
        size_t entries_in_block = entries_remaining < DIRECTORY_ENTRIES_PER_BLOCK ? entries_remaining : DIRECTORY_ENTRIES_PER_BLOCK;

        for (size_t i = 0; i < entries_in_block; i++) {
            if (memchr(entries[i].name, '\0', DIRECTORY_NAME_SIZE) == NULL) {
                return FS_ERR_INVALID_DISK;
            }
            if (entries[i].inode_number >= MAXIMUM_INODES) {
                return FS_ERR_INVALID_DISK;
            }
            if (strcmp(entries[i].name, name) == 0) {
                if (inode_number != NULL) {
                    *inode_number = entries[i].inode_number;
                }
                if (logical_directory_entry != NULL) {
                    *logical_directory_entry = entries_processed + i;
                }
                if (physical_block != NULL) {
                    *physical_block = block_to_process;
                }
                if (entry_index != NULL) {
                    *entry_index = i;
                }
                return FS_OK;
            }
        }
        entries_processed += entries_in_block;
    }
    return FS_ERR_ENTRY_NOT_FOUND;
}

int directory_find_entry(Disk *disk, uint32_t dir_inode_number, const char *name, uint32_t *inode_number) {
    if (disk == NULL || name == NULL || inode_number == NULL) {
        return FS_ERR_NULL;
    }
    if (dir_inode_number >= MAXIMUM_INODES) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }
    size_t name_length = strlen(name);
    if (name_length == 0 || name_length > MAX_FILENAME_LENGTH) {
        return FS_ERR_INVALID_FILE_NAME;
    }
    Inode dir_inode;
    int res = inode_table_read(disk, dir_inode_number, &dir_inode);
    if (res != FS_OK) {
        return res;
    }
    return directory_find_entry_internal(disk, &dir_inode, name, inode_number, NULL, NULL, NULL);
}

int directory_add_entry(Disk *disk, uint32_t dir_inode_number, const char *name, uint32_t inode_number) {
    if (disk == NULL || name == NULL) {
        return FS_ERR_NULL;
    }
    if (dir_inode_number >= MAXIMUM_INODES || inode_number >= MAXIMUM_INODES) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }
    size_t name_length = strlen(name);
    if (name_length == 0 || name_length > MAX_FILENAME_LENGTH) {
        return FS_ERR_INVALID_FILE_NAME;
    }

    Inode dir_inode;
    int res = inode_table_read(disk, dir_inode_number, &dir_inode);
    if (res != FS_OK) {
        return res;
    }
    if (dir_inode.type != INODE_TYPE_DIRECTORY) {
        return FS_ERR_ENTRY_IS_NOT_DIRECTORY;
    }
    if (dir_inode.size % DIRECTORY_ENTRY_SIZE != 0) {
        return FS_ERR_INVALID_DISK;
    }

    uint32_t existing_inode;
    res = directory_find_entry(disk, dir_inode_number, name, &existing_inode);
    if (res == FS_OK) {
        return FS_ERR_ENTRY_ALREADY_EXISTS;
    }
    if (res != FS_ERR_ENTRY_NOT_FOUND) {
        return res;
    }

    size_t entries_count = dir_inode.size / DIRECTORY_ENTRY_SIZE;
    size_t logical_block_index = entries_count / DIRECTORY_ENTRIES_PER_BLOCK;
    size_t entry_index = entries_count % DIRECTORY_ENTRIES_PER_BLOCK;
    uint32_t physical_block;
    bool created_data_block, created_indirect_block;
    res = directory_ensure_data_block(disk, &dir_inode, logical_block_index, &physical_block, &created_data_block, &created_indirect_block);
    if (res != FS_OK) {
        return res;
    }

    DirectoryEntry entries[DIRECTORY_ENTRIES_PER_BLOCK];
    res = disk_read(disk, entries, physical_block);
    if (res != FS_OK) {
        directory_rollback_data_block(disk, &dir_inode, logical_block_index, physical_block, created_data_block, created_indirect_block);
        return res;
    }

    DirectoryEntry new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    new_entry.inode_number = inode_number;
    memcpy(new_entry.name, name, name_length + 1);
    entries[entry_index] = new_entry;
    res = disk_write(disk, entries, physical_block);
    if (res != FS_OK) {
        directory_rollback_data_block(disk, &dir_inode, logical_block_index, physical_block, created_data_block, created_indirect_block);
        return res;
    }

    dir_inode.size += DIRECTORY_ENTRY_SIZE;
    res = inode_table_write(disk, dir_inode_number, &dir_inode);
    if (res != FS_OK) {
        directory_rollback_data_block(disk, &dir_inode, logical_block_index, physical_block, created_data_block, created_indirect_block);
        return res;
    }

    return FS_OK;
}

int directory_remove_entry(Disk *disk, uint32_t dir_inode_number, const char *name) {
    if (disk == NULL || name == NULL) {
        return FS_ERR_NULL;
    }
    if (dir_inode_number >= MAXIMUM_INODES) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }

    Inode dir_inode;
    int res = inode_table_read(disk, dir_inode_number, &dir_inode);
    if (res != FS_OK) {
        return res;
    }

    size_t target_logical_directory_entry;
    uint32_t target_physical_block;
    size_t target_entry_index;
    res = directory_find_entry_internal(disk, &dir_inode, name, NULL, &target_logical_directory_entry, &target_physical_block, &target_entry_index);
    if (res != FS_OK) {
        return res;
    }

    size_t entries_count = dir_inode.size / DIRECTORY_ENTRY_SIZE;

    size_t last_logical_directory_entry = entries_count - 1;
    size_t last_logical_block = last_logical_directory_entry / DIRECTORY_ENTRIES_PER_BLOCK;
    size_t last_entry_index = last_logical_directory_entry % DIRECTORY_ENTRIES_PER_BLOCK;

    uint32_t last_physical_block;

    bool last_block_is_indirect = false;
    size_t last_indirect_index = 0;

    uint32_t indirect_pointers[INDIRECT_POINTERS_PER_BLOCK];
    if (last_logical_block < INODE_DIRECT_POINTERS_COUNT) {
        last_physical_block = dir_inode.direct_blocks[last_logical_block];
    } else {
        last_block_is_indirect = true;
        if (dir_inode.indirect_block < USER_DATA_BLOCK_START || dir_inode.indirect_block >= MAXIMUM_BLOCKS) {
            return FS_ERR_INVALID_BLOCK;
        }
        res = disk_read(disk, indirect_pointers, dir_inode.indirect_block);
        if (res != FS_OK) {
            return res;
        }
        last_indirect_index = last_logical_block - INODE_DIRECT_POINTERS_COUNT;
        last_physical_block = indirect_pointers[last_indirect_index];
    }

    if (last_physical_block < USER_DATA_BLOCK_START || last_physical_block >= MAXIMUM_BLOCKS) {
        return FS_ERR_INVALID_BLOCK;
    }

    DirectoryEntry last_entries[DIRECTORY_ENTRIES_PER_BLOCK];
    res = disk_read(disk, last_entries, last_physical_block);
    if (res != FS_OK) {
        return res;
    }

    DirectoryEntry last_entry = last_entries[last_entry_index];
    if (memchr(last_entry.name, '\0', DIRECTORY_NAME_SIZE) == NULL) {
        return FS_ERR_INVALID_DISK;
    }
    if (last_entry.inode_number >= MAXIMUM_INODES) {
        return FS_ERR_INVALID_DISK;
    }

    // Save the old target so we can restore it if inode_table_write() fails.
    DirectoryEntry original_target;
    bool target_was_modified = false;
    if (target_logical_directory_entry != last_logical_directory_entry) {
        if (target_physical_block == last_physical_block) { // Target and last entry are in the same physical block.
            original_target = last_entries[target_entry_index];
            last_entries[target_entry_index] = last_entry;
            res = disk_write(disk, last_entries, last_physical_block);

            if (res != FS_OK) {
                return res;
            }
        } else { // Target and last entry are in different blocks.
            DirectoryEntry target_entries[DIRECTORY_ENTRIES_PER_BLOCK];
            res = disk_read(disk, target_entries, target_physical_block);
            if (res != FS_OK) {
                return res;
            }
            original_target = target_entries[target_entry_index];
            target_entries[target_entry_index] = last_entry;
            res = disk_write(disk, target_entries, target_physical_block);
            if (res != FS_OK) {
                return res;
            }
        }
        target_was_modified = true;
    }

    bool release_last_block = last_entry_index == 0 && last_logical_block > 0;
    if (release_last_block && !last_block_is_indirect) {
        dir_inode.direct_blocks[last_logical_block] = INVALID_BLOCK;
    }
    /*
     * If this is the FIRST indirect data block, removing it
     * means the directory no longer needs indirect addressing
     * at all.
     */
    bool release_indirect_block = false;
    uint32_t indirect_block_to_free = INVALID_BLOCK;
    if (release_last_block && last_block_is_indirect && last_indirect_index == 0) {
        indirect_block_to_free = dir_inode.indirect_block;
        dir_inode.indirect_block = INVALID_BLOCK;
        release_indirect_block = true;
    }
    // Shrink the visible directory.
    dir_inode.size -= DIRECTORY_ENTRY_SIZE;

    // Commit the new directory metadata.
    res = inode_table_write(disk, dir_inode_number, &dir_inode);
    if (res != FS_OK) {
        // We overwrote the target entry, but the old inodesize is still on disk. Restore the original target entry.
        if (target_was_modified) {
            DirectoryEntry entries[DIRECTORY_ENTRIES_PER_BLOCK];
            if (disk_read(disk, entries, target_physical_block) == FS_OK) {
                entries[target_entry_index] = original_target;
                disk_write(disk, entries, target_physical_block);
            }
        }
        return res;
    }

    // No block became unused.

    if (!release_last_block) {
        return FS_OK;
    }

    /*
     * DIRECT BLOCK
     * The inode no longer points to this block, so it is now
     * safe to release it from the bitmap.
     */
    if (!last_block_is_indirect) {
        return fs_free_data_block(disk, last_physical_block);
    }

    /*
     * FIRST INDIRECT DATA BLOCK
     * There are now no indirect data blocks at all.
     * Free:
     * 1. the data block
     * 2. the indirect-pointer block
     */
    if (release_indirect_block) {
        int data_res = fs_free_data_block(disk, last_physical_block);
        int indirect_res = fs_free_data_block(disk, indirect_block_to_free);
        if (data_res != FS_OK) {
            return data_res;
        }
        return indirect_res;
    }

    /*
     * A later indirect data block became unused.
     * The indirect-pointer block must stay because earlier
     * indirect data blocks still exist.
     */
    indirect_pointers[last_indirect_index] = INVALID_BLOCK;
    res = disk_write(disk, indirect_pointers, dir_inode.indirect_block);
    if (res != FS_OK) {
        return res;
    }
    return fs_free_data_block(disk, last_physical_block);
}