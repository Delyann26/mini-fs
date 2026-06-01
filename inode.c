#include "inode.h"
#include <stddef.h>

int inode_init(Inode *inode, InodeType type) {
    if (inode == NULL || type > INODE_TYPE_DIRECTORY) {
        return -1;
    }
    inode->type = type;
    inode->size = 0;
    for (size_t i = 0; i < INODE_DIRECT_POINTERS_COUNT; i++) {
        inode->direct_blocks[i] = INVALID_BLOCK;
    }
    inode->indirect_block = INVALID_BLOCK;
    return 0;
}