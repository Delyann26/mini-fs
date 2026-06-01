#include "inode_table.h"
#include <string.h>
int inode_table_read(Disk *disk, uint32_t inode_number, Inode *inode) {
    if (disk == NULL || inode == NULL || inode_number >= MAXIMUM_INODES) {
        return -1;
    }
    uint32_t block_index = INODE_TABLE_BLOCK_START + inode_number / INODES_PER_BLOCK;
    uint32_t offset = (inode_number % INODES_PER_BLOCK) * INODE_SIZE;
    uint8_t buffer[BLOCK_SIZE];
    if (disk_read_block(disk, block_index, buffer) == -1) {
        return -1;
    }
    memcpy(inode, buffer + offset, sizeof(*inode));
    return 0;
}

int inode_table_write(Disk *disk, uint32_t inode_number, const Inode *inode) {
    if (disk == NULL || inode == NULL || inode_number >= MAXIMUM_INODES) {
        return -1;
    }
    uint32_t block_index = INODE_TABLE_BLOCK_START + inode_number / INODES_PER_BLOCK;
    uint32_t offset = (inode_number % INODES_PER_BLOCK) * INODE_SIZE;
    uint8_t buffer[BLOCK_SIZE];
    if (disk_read_block(disk, block_index, buffer) == -1) {
        return -1;
    }
    memcpy(buffer + offset, inode, sizeof(*inode));
    if (disk_write_block(disk, block_index, buffer) == -1) {
        return -1;
    }
    return 0;
}