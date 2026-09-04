#ifndef FS_ALLOC_H
#define FS_ALLOC_H

#include "disk.h"
#include <stdint.h>

int fs_allocate_inode(Disk *disk, uint32_t *inode_number);
int fs_free_inode(Disk *disk, uint32_t inode_number);

int fs_allocate_data_block(Disk *disk, uint32_t *block_number);
int fs_free_data_block(Disk *disk, uint32_t block_number);

#endif