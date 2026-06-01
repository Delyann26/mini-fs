#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include "common.h"
#include "disk.h"
#include <stdbool.h>

typedef struct {
    uint32_t magic;          // identifies this as your filesystem
    uint32_t block_size;     // size of one block, e.g. 4096
    uint32_t maximum_blocks; // total number of blocks on disk

    uint32_t inode_bitmap_block; // block where inode bitmap is stored
    uint32_t data_bitmap_block;  // block where data block bitmap is stored

    uint32_t inode_table_block_start; // first block of inode table
    uint32_t maximum_inode_blocks;    // total amount of inode table blocks
    uint32_t maximum_inodes;          // maximum number of inodes

    uint32_t data_block_start;    // first block of user data
    uint32_t maximum_data_blocks; // total amount of user data blocks

    uint32_t root_inode; // inode number of root directory
} SuperBlock;

int superblock_init(SuperBlock *sb);
bool superblock_is_valid(const SuperBlock *sb);

int superblock_read(Disk *disk, SuperBlock *sb);
int superblock_write(Disk *disk, const SuperBlock *sb);

#endif