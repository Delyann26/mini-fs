#include "superblock.h"
#include <string.h>

int superblock_init(SuperBlock *sb) {
    if (!sb) {
        return -1;
    }

    sb->magic = FS_MAGIC;
    sb->block_size = BLOCK_SIZE;
    sb->maximum_blocks = MAXIMUM_BLOCKS;

    sb->inode_bitmap_block = INODE_BITMAP_BLOCK;
    sb->data_bitmap_block = DATA_BITMAP_BLOCK;

    sb->inode_table_block_start = INODE_TABLE_BLOCK_START;
    sb->maximum_inode_blocks = MAXIMUM_INODE_BLOCKS;
    sb->maximum_inodes = MAXIMUM_INODES;

    sb->data_block_start = DATA_BLOCK_START;
    sb->maximum_data_blocks = MAXIMUM_DATA_BLOCKS;

    sb->root_inode = ROOT_INODE;

    return 0;
}

bool superblock_is_valid(const SuperBlock *sb) {
    if (sb == NULL) {
        return false;
    }
    return sb->magic == FS_MAGIC &&
           sb->block_size == BLOCK_SIZE &&
           sb->maximum_blocks == MAXIMUM_BLOCKS &&
           sb->inode_bitmap_block == INODE_BITMAP_BLOCK &&
           sb->data_bitmap_block == DATA_BITMAP_BLOCK &&
           sb->inode_table_block_start == INODE_TABLE_BLOCK_START &&
           sb->maximum_inode_blocks == MAXIMUM_INODE_BLOCKS &&
           sb->maximum_inodes == MAXIMUM_INODES &&
           sb->data_block_start == DATA_BLOCK_START &&
           sb->maximum_data_blocks == MAXIMUM_DATA_BLOCKS &&
           sb->root_inode == ROOT_INODE;
}

int superblock_read(Disk *disk, SuperBlock *sb) {
    if (disk == NULL || sb == NULL) {
        return -1;
    }
    uint8_t buffer[BLOCK_SIZE];
    if (disk_read_block(disk, SUPERBLOCK_BLOCK, buffer) == -1) {
        return -1;
    }
    memcpy(sb, buffer, sizeof(*sb));
    return 0;
}

int superblock_write(Disk *disk, const SuperBlock *sb) {
    if (disk == NULL || sb == NULL) {
        return -1;
    }
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, sb, sizeof(*sb));
    if (disk_write_block(disk, SUPERBLOCK_BLOCK, buffer) == -1) {
        return -1;
    }
    return 0;
}