#include "filesystem.h"
#include "bitmap.h"
#include "common.h"
#include "directory.h"
#include "fs_alloc.h"
#include "inode.h"
#include "inode_table.h"
#include "path.h"
#include "string.h"
#include <unistd.h>

int filesystem_init(FileSystem *fs) {
    if (fs == NULL) {
        return FS_ERR_NULL;
    }
    int res = disk_init(&fs->disk);
    if (res != FS_OK) {
        return res;
    }
    memset(&fs->superblock, 0, sizeof(fs->superblock));
    fs->mounted = false;
    return FS_OK;
}

int filesystem_format(const char *path) {
    if (path == NULL) {
        return FS_ERR_NULL;
    }
    int res;

    Disk disk;
    res = disk_init(&disk);
    if (res != FS_OK) {
        return res;
    }
    res = disk_create(&disk, path);
    if (res != FS_OK) {
        return res;
    }

    uint8_t inode_bitmap[BLOCK_SIZE];
    memset(inode_bitmap, 0, sizeof(inode_bitmap));
    res = bitmap_set_bit(inode_bitmap, MAXIMUM_INODES, ROOT_INODE);
    if (res != FS_OK) {
        disk_close(&disk);
        unlink(path);
        return res;
    }
    res = disk_write(&disk, inode_bitmap, INODE_BITMAP_BLOCK);
    if (res != FS_OK) {
        disk_close(&disk);
        unlink(path);
        return res;
    }

    uint32_t root_data_block;
    res = fs_allocate_data_block(&disk, &root_data_block);
    if (res != FS_OK) {
        disk_close(&disk);
        unlink(path);
        return res;
    }

    Inode root_inode;
    res = inode_init(&root_inode);
    if (res != FS_OK) {
        disk_close(&disk);
        unlink(path);
        return res;
    }
    root_inode.direct_blocks[0] = root_data_block;
    root_inode.type = INODE_TYPE_DIRECTORY;
    res = inode_table_write(&disk, ROOT_INODE, &root_inode);
    if (res != FS_OK) {
        disk_close(&disk);
        unlink(path);
        return res;
    }

    SuperBlock superblock;
    res = superblock_init(&superblock);
    if (res != FS_OK) {
        disk_close(&disk);
        unlink(path);
        return res;
    }
    res = superblock_write(&superblock, &disk);
    if (res != FS_OK) {
        disk_close(&disk);
        unlink(path);
        return res;
    }

    res = disk_close(&disk);
    if (res != FS_OK) {
        unlink(path);
        return res;
    }
    return FS_OK;
}

int filesystem_mount(FileSystem *fs, const char *path) {
    if (fs == NULL || path == NULL) {
        return FS_ERR_NULL;
    }
    if (fs->mounted) {
        return FS_ERR_ALREADY_MOUNTED;
    }
    int res;
    res = disk_open(&fs->disk, path);
    if (res != FS_OK) {
        return res;
    }
    res = superblock_read(&fs->superblock, &fs->disk);
    if (res != FS_OK) {
        disk_close(&fs->disk);
        memset(&fs->superblock, 0, sizeof(fs->superblock));
        return res;
    }
    fs->mounted = true;
    return FS_OK;
}

int filesystem_unmount(FileSystem *fs) {
    if (fs == NULL) {
        return FS_ERR_NULL;
    }
    if (!fs->mounted) {
        return FS_ERR_NOT_MOUNTED;
    }
    int res = disk_close(&fs->disk);
    memset(&fs->superblock, 0, sizeof(fs->superblock));
    fs->mounted = false;
    return res;
}

int filesystem_create_file(FileSystem *fs, const char *path) {
    if (fs == NULL || path == NULL) {
        return FS_ERR_NULL;
    }
    if (fs->mounted == false) {
        return FS_ERR_FILESYSTEM_NOT_MOUNTED;
    }
    uint32_t parent_inode_number;
    char new_file_name[DIRECTORY_NAME_SIZE];
    int res = resolve_parent_path(&fs->disk, path, &parent_inode_number, new_file_name);
    if (res != FS_OK) {
        return res;
    }
    uint32_t new_file_inode_number;
    res = directory_find_entry(&fs->disk, parent_inode_number, new_file_name, &new_file_inode_number);
    if (res == FS_OK) {
        return FS_ERR_ENTRY_ALREADY_EXISTS;
    }
    if (res != FS_ERR_ENTRY_NOT_FOUND) {
        return res;
    }
    res = fs_allocate_inode(&fs->disk, &new_file_inode_number);
    if (res != FS_OK) {
        return res;
    }
    Inode inode;
    res = inode_init(&inode);
    if (res != FS_OK) {
        fs_free_inode(&fs->disk, new_file_inode_number);
        return res;
    }
    inode.type = INODE_TYPE_FILE;
    res = inode_table_write(&fs->disk, new_file_inode_number, &inode);
    if (res != FS_OK) {
        fs_free_inode(&fs->disk, new_file_inode_number);
        return res;
    }
    res = directory_add_entry(&fs->disk, parent_inode_number, new_file_name, new_file_inode_number);
    if (res != FS_OK) {
        inode_init(&inode);
        inode_table_write(&fs->disk, new_file_inode_number, &inode);
        fs_free_inode(&fs->disk, new_file_inode_number);
        return res;
    }
    return FS_OK;
}

int filesystem_create_directory(FileSystem *fs, const char *path) {
    if (fs == NULL || path == NULL) {
        return FS_ERR_NULL;
    }
    if (fs->mounted == false) {
        return FS_ERR_FILESYSTEM_NOT_MOUNTED;
    }
    uint32_t parent_inode_number;
    char new_directory_name[DIRECTORY_NAME_SIZE];
    int res = resolve_parent_path(&fs->disk, path, &parent_inode_number, new_directory_name);
    if (res != FS_OK) {
        return res;
    }
    uint32_t new_directory_inode_number;
    uint32_t new_directory_data_block_number;
    res = directory_find_entry(&fs->disk, parent_inode_number, new_directory_name, &new_directory_inode_number);
    if (res == FS_OK) {
        return FS_ERR_ENTRY_ALREADY_EXISTS;
    }
    if (res != FS_ERR_ENTRY_NOT_FOUND) {
        return res;
    }
    res = fs_allocate_inode(&fs->disk, &new_directory_inode_number);
    if (res != FS_OK) {
        return res;
    }
    res = fs_allocate_data_block(&fs->disk, &new_directory_data_block_number);
    if (res != FS_OK) {
        fs_free_inode(&fs->disk, new_directory_inode_number);
        return res;
    }
    uint8_t empty_block[BLOCK_SIZE];
    memset(empty_block, 0, sizeof(empty_block));
    res = disk_write(&fs->disk, empty_block, new_directory_data_block_number);
    if (res != FS_OK) {
        fs_free_data_block(&fs->disk, new_directory_data_block_number);
        fs_free_inode(&fs->disk, new_directory_inode_number);
        return res;
    }
    Inode inode;
    res = inode_init(&inode);
    if (res != FS_OK) {
        fs_free_data_block(&fs->disk, new_directory_data_block_number);
        fs_free_inode(&fs->disk, new_directory_inode_number);
        return res;
    }
    inode.direct_blocks[0] = new_directory_data_block_number;
    inode.type = INODE_TYPE_DIRECTORY;
    res = inode_table_write(&fs->disk, new_directory_inode_number, &inode);
    if (res != FS_OK) {
        fs_free_data_block(&fs->disk, new_directory_data_block_number);
        fs_free_inode(&fs->disk, new_directory_inode_number);
        return res;
    }
    res = directory_add_entry(&fs->disk, parent_inode_number, new_directory_name, new_directory_inode_number);
    if (res != FS_OK) {
        inode_init(&inode);
        inode_table_write(&fs->disk, new_directory_inode_number, &inode);
        fs_free_data_block(&fs->disk, new_directory_data_block_number);
        fs_free_inode(&fs->disk, new_directory_inode_number);
        return res;
    }
    return FS_OK;
}
