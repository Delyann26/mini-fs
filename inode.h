#ifndef INODE_H
#define INODE_H
#include "common.h"

typedef enum {
    INODE_TYPE_FREE = 0,
    INODE_TYPE_FILE = 1,
    INODE_TYPE_DIRECTORY = 2
} InodeType;

typedef struct {
    uint32_t type;
    uint32_t size;

    uint32_t direct_blocks[INODE_DIRECT_POINTERS_COUNT];
    uint32_t indirect_block;
} Inode;

int inode_init(Inode *inode, InodeType type);

#endif
