#ifndef INODE_H
#define INODE_H
#include <common.h>
#include <stdint.h>

typedef enum {
    FREE = 0,
    FILE = 1,
    DIRECTORY = 2
} InodeType;

typedef struct {
    uint32_t type;
    uint32_t file_size;

    uint32_t direct_blocks[INODE_DIRECT_POINTERS_COUNT];
    uint32_t indirect_block;
} Inode;
// TODO: add more characteristics like permissions etc

#endif
