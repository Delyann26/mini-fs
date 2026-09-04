#ifndef INODE_H
#define INODE_H
#include "common.h"
#include <stdint.h>

typedef enum { INODE_TYPE_FREE = 0, INODE_TYPE_FILE = 1, INODE_TYPE_DIRECTORY = 2 } InodeType;

typedef struct Inode {
    uint32_t type;
    uint32_t size;

    // An inode data-block pointer is valid only if it lies in [USER_DATA_BLOCK_START,
    // MAXIMUM_BLOCKS - 1]; 0 means unassigned.
    uint32_t direct_blocks[INODE_DIRECT_POINTERS_COUNT];
    uint32_t indirect_block;
} Inode;

int inode_init(Inode *inode);

#endif