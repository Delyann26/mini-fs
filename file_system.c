#include "file_system.h"
#include "bitmap.h"
#include "inode_table.h"
#include <string.h>

int filesystem_init(FileSystem *fs) {
    if (fs == NULL) {
        return -1;
    }
    if (disk_init(&fs->disk) == -1) {
        return -1;
    }
    memset(&fs->superblock, 0, sizeof(fs->superblock));
    fs->mounted = 0;

    return 0;
}

int filesystem_format(const char *path) {
    if (path == NULL) {
        return -1;
    }

    Disk disk;
    disk_init(&disk);
    if (disk_create(&disk, path) == -1) {
        return -1;
    }

    SuperBlock sb;
    if (superblock_init(&sb) == -1) {
        disk_close(&disk);
        return -1;
    }
    if (superblock_write(&disk, &sb) == -1) {
        disk_close(&disk);
        return -1;
    }

    uint8_t inode_bitmap[BLOCK_SIZE];
    memset(inode_bitmap, 0, sizeof(inode_bitmap));
    if (bitmap_set(inode_bitmap, ROOT_INODE) == -1) {
        disk_close(&disk);
        return -1;
    }
    if (disk_write_block(&disk, INODE_BITMAP_BLOCK, inode_bitmap) == -1) {
        disk_close(&disk);
        return -1;
    }

    uint8_t data_bitmap[BLOCK_SIZE];
    memset(data_bitmap, 0, sizeof(data_bitmap));
    if (disk_write_block(&disk, DATA_BITMAP_BLOCK, data_bitmap) == -1) {
        disk_close(&disk);
        return -1;
    }

    Inode root;
    if (inode_init(&root, INODE_TYPE_DIRECTORY) == -1) {
        disk_close(&disk);
        return -1;
    }
    if (inode_table_write(&disk, ROOT_INODE, &root) == -1) {
        disk_close(&disk);
        return -1;
    }

    if (disk_close(&disk) == -1) {
        return -1;
    }
    return 0;
}

int filesystem_mount(FileSystem *fs, const char *path) {
    if (fs == NULL || path == NULL || fs->mounted == 1) {
        return -1;
    }
    if (disk_open(&fs->disk, path) == -1) {
        return -1;
    }
    if (superblock_read(&fs->disk, &fs->superblock) == -1) {
        disk_close(&fs->disk);
        return -1;
    }
    if (!superblock_is_valid(&fs->superblock)) {
        disk_close(&fs->disk);
        return -1;
    }
    fs->mounted = 1;
    return 0;
}

int filesystem_unmount(FileSystem *fs) {
    if (fs == NULL || fs->mounted != 1) {
        return -1;
    }
    if (disk_close(&fs->disk) == -1) {
        return -1;
    }
    memset(&fs->superblock, 0, sizeof(fs->superblock));
    fs->mounted = 0;

    return 0;
}