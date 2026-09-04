#include "inode_table.h"
#include <string.h>
int inode_table_read(Disk *disk, uint32_t inode_number, Inode *inode) {
    if (disk == NULL || inode == NULL) {
        return FS_ERR_NULL;
    }
    if (inode_number >= MAXIMUM_INODES) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }
    uint32_t block = (inode_number / INODES_PER_BLOCK) + INODE_TABLE_BLOCK_START;
    uint32_t offset = (inode_number % INODES_PER_BLOCK) * INODE_SIZE;
    uint8_t block_bytes[BLOCK_SIZE];

    int res = disk_read(disk, block_bytes, block);
    if (res != FS_OK) {
        return res;
    }
    memcpy(inode, block_bytes + offset, sizeof(*inode));

    return FS_OK;
}

int inode_table_write(Disk *disk, uint32_t inode_number, const Inode *inode) {
    if (disk == NULL || inode == NULL) {
        return FS_ERR_NULL;
    }
    if (inode_number >= MAXIMUM_INODES) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }
    uint32_t block = (inode_number / INODES_PER_BLOCK) + INODE_TABLE_BLOCK_START;
    uint32_t offset = (inode_number % INODES_PER_BLOCK) * INODE_SIZE;
    uint8_t block_bytes[BLOCK_SIZE];

    int res = disk_read(disk, block_bytes, block);
    if (res != FS_OK) {
        return res;
    }
    memset(block_bytes + offset, 0, INODE_SIZE);
    memcpy(block_bytes + offset, inode, sizeof(*inode));
    res = disk_write(disk, block_bytes, block);
    if (res != FS_OK) {
        return res;
    }

    return FS_OK;
}