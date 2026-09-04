#include "superblock.h"
#include "common.h"
#include <string.h>

int superblock_init(SuperBlock *block) {
    if (block == NULL) {
        return FS_ERR_NULL;
    }

    block->magic = FS_MAGIC;
    block->block_size = BLOCK_SIZE;
    block->total_blocks = MAXIMUM_BLOCKS;
    block->inode_bitmap_block = INODE_BITMAP_BLOCK;
    block->data_bitmap_block = DATA_BITMAP_BLOCK;
    block->inode_table_block_start = INODE_TABLE_BLOCK_START;
    block->total_inode_table_blocks = TOTAL_INODE_TABLE_BLOCKS;
    block->maximum_inodes = MAXIMUM_INODES;
    block->user_data_block_start = USER_DATA_BLOCK_START;
    block->total_user_data_blocks = TOTAL_USER_DATA_BLOCKS;
    block->root_inode = ROOT_INODE;

    return FS_OK;
}

bool superblock_is_valid(const SuperBlock *block) {
    if (block == NULL) {
        return false;
    }
    return block->magic == FS_MAGIC && block->block_size == BLOCK_SIZE && block->total_blocks == MAXIMUM_BLOCKS &&
           block->inode_bitmap_block == INODE_BITMAP_BLOCK && block->data_bitmap_block == DATA_BITMAP_BLOCK &&
           block->inode_table_block_start == INODE_TABLE_BLOCK_START && block->total_inode_table_blocks == TOTAL_INODE_TABLE_BLOCKS &&
           block->maximum_inodes == MAXIMUM_INODES && block->user_data_block_start == USER_DATA_BLOCK_START &&
           block->total_user_data_blocks == TOTAL_USER_DATA_BLOCKS && block->root_inode == ROOT_INODE;
}

int superblock_write(const SuperBlock *block, Disk *disk) {
    if (block == NULL || disk == NULL) {
        return FS_ERR_NULL;
    }
    if (!superblock_is_valid(block)) {
        return FS_ERR_INVALID_DISK;
    }

    uint8_t temp_buffer[BLOCK_SIZE];
    memset(temp_buffer, 0, sizeof(temp_buffer));
    memcpy(temp_buffer, block, sizeof(*block));

    int res = disk_write(disk, temp_buffer, SUPERBLOCK_BLOCK);
    if (res != FS_OK) {
        return res;
    }
    return FS_OK;
}

int superblock_read(SuperBlock *block, Disk *disk) {
    if (block == NULL || disk == NULL) {
        return FS_ERR_NULL;
    }
    uint8_t temp_buffer[BLOCK_SIZE];
    int res = disk_read(disk, temp_buffer, SUPERBLOCK_BLOCK);
    if (res != FS_OK) {
        return res;
    }
    memcpy(block, temp_buffer, sizeof(*block));

    if (!superblock_is_valid(block)) {
        return FS_ERR_INVALID_DISK;
    }
    return FS_OK;
}
