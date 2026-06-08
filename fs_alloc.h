#ifndef FS_ALLOC_H
#define FS_ALLOC_H

#include "common.h"
#include "disk.h"

uint32_t fs_allocate_inode(Disk *disk);
int fs_free_inode(Disk *disk, uint32_t inode_index);

uint32_t fs_allocate_data_block(Disk *disk);
int fs_free_data_block(Disk *disk, uint32_t block_index);

#endif