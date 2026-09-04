#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H
#include <disk.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct SuperBlock {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_bitmap_block;
    uint32_t data_bitmap_block;
    uint32_t inode_table_block_start;
    uint32_t total_inode_table_blocks;
    uint32_t maximum_inodes;
    uint32_t user_data_block_start;
    uint32_t total_user_data_blocks;
    uint32_t root_inode;
} SuperBlock;

int superblock_init(SuperBlock *block);
bool superblock_is_valid(const SuperBlock *block);

int superblock_write(const SuperBlock *block, Disk *disk);
int superblock_read(SuperBlock *block, Disk *disk);

#endif
