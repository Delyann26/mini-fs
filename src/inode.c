#include "inode.h"
#include <string.h>
int inode_init(Inode *inode) {
    if (inode == NULL) {
        return FS_ERR_NULL;
    }
    inode->type = INODE_TYPE_FREE;
    inode->size = 0;
    memset(inode->direct_blocks, INVALID_BLOCK, sizeof(inode->direct_blocks));
    inode->indirect_block = INVALID_BLOCK;
    return FS_OK;
}