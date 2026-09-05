#include "directory.h"
#include "fs_alloc.h"
#include "inode_table.h"
#include "math.h"
#include <string.h>

static int directory_ensure_data_block(Disk *disk, Inode *dir_inode, size_t logical_block_index, uint32_t *physical_block) {
    size_t maximum_blocks_supported = INODE_DIRECT_POINTERS_COUNT + INDIRECT_POINTERS_PER_BLOCK;
    if (logical_block_index >= maximum_blocks_supported) {
        return FS_ERR_DIRECTORY_IS_FULL;
    }

    int res;
    if (logical_block_index < INODE_DIRECT_POINTERS_COUNT) { // DIRECT
        uint32_t block = dir_inode->direct_blocks[logical_block_index];
        if (block == INVALID_BLOCK) {
            res = fs_allocate_data_block(disk, &block);
            if (res != FS_OK) {
                return res;
            }
            uint8_t empty_block[BLOCK_SIZE];
            memset(empty_block, 0, sizeof(empty_block));
            res = disk_write(disk, empty_block, block);
            if (res != FS_OK) {
                fs_free_data_block(disk, block);
                return res;
            }
            dir_inode->direct_blocks[logical_block_index] = block;
        }
        if (block < USER_DATA_BLOCK_START || block >= MAXIMUM_BLOCKS) {
            return FS_ERR_INVALID_BLOCK;
        }
        *physical_block = block;
        return FS_OK;
    }

    // INDIRECT
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
    if (dir_inode.type != INODE_TYPE_DIRECTORY) {
        return FS_ERR_ENTRY_IS_NOT_DIRECTORY;
    }
    if (dir_inode.size % DIRECTORY_ENTRY_SIZE != 0) {
        return FS_ERR_INVALID_DISK;
    }
    size_t entries_count = dir_inode.size / DIRECTORY_ENTRY_SIZE;
    if (entries_count == 0) {
        return FS_ERR_ENTRY_NOT_FOUND;
    }
    size_t blocks_needed = (size_t)ceil((double)entries_count / DIRECTORY_ENTRIES_PER_BLOCK);
    size_t maximum_blocks_needed = INODE_DIRECT_POINTERS_COUNT + INDIRECT_POINTERS_PER_BLOCK;
    if (blocks_needed > maximum_blocks_needed) {
        return FS_ERR_INVALID_DISK;
    }

    uint32_t indirect_pointers[INDIRECT_POINTERS_PER_BLOCK];
    if (blocks_needed > INODE_DIRECT_POINTERS_COUNT) {
        if (dir_inode.indirect_block < USER_DATA_BLOCK_START || dir_inode.indirect_block >= MAXIMUM_BLOCKS) {
            return FS_ERR_INVALID_BLOCK;
        }
        res = disk_read(disk, indirect_pointers, dir_inode.indirect_block);
        if (res != FS_OK) {
            return res;
        }
    }

    size_t entries_processed = 0;
    for (size_t block_index = 0; block_index < blocks_needed; block_index++) {
        uint32_t block_to_process;
        if (block_index < INODE_DIRECT_POINTERS_COUNT) {
            block_to_process = dir_inode.direct_blocks[block_index];
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
                *inode_number = entries[i].inode_number;
                return FS_OK;
            }
        }
        entries_processed += entries_in_block;
    }
    return FS_ERR_ENTRY_NOT_FOUND;
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
    res = directory_ensure_data_block(disk, &dir_inode, logical_block_index, &physical_block);
    if (res != FS_OK) {
        return res;
    }

    DirectoryEntry entries[DIRECTORY_ENTRIES_PER_BLOCK];
    res = disk_read(disk, entries, physical_block);
    if (res != FS_OK) {
        return res;
    }

    DirectoryEntry new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    new_entry.inode_number = inode_number;
    memcpy(new_entry.name, name, name_length + 1);
    entries[entry_index] = new_entry;
    res = disk_wirte(disk, entries, physical_block);
    if (res != FS_OK) {
        return res;
    }

    dir_inode.size += DIRECTORY_ENTRY_SIZE;
    res = inode_table_write(disk, dir_inode_number, &dir_inode);
    if (res != FS_OK) {
        return res;
    }

    return FS_OK;
}