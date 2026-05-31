#include "superblock.h"
#include "common.h"
#include <stdlib.h>

int superblock_init(SuperBlock *sb) {
    if (!sb) {
        return FS_ERROR;
    }

    sb->magic = FS_MAGIC;
    sb->block_size = BLOCK_SIZE;
    sb->total_blocks = MAXIMUM_BLOCKS;

    sb->inode_bitmap_block = INODE_BITMAP_BLOCK;
    sb->user_data_bitmap_block = DATA_BITMAP_BLOCK;

    sb->inode_table_block_start = INODE_TABLE_BLOCK_START;
    sb->inode_table_blocks = MAXIMUM_INODE_BLOCKS;
    sb->maximum_inodes = MAXIMUM_INODES;

    sb->user_data_block_start = DATA_BLOCK_START;
    sb->user_data_blocks = MAXIMUM_DATA_BLOCKS;

    sb->root_inode = ROOT_INODE;

    return FS_SUCCESS;
}