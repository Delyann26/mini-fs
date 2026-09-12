#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "inode.h"
#include <common.h>
#include <disk.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct DirectoryEntry {
    uint32_t inode_number;
    char name[DIRECTORY_NAME_SIZE];
} DirectoryEntry;

int directory_ensure_data_block(Disk *disk, Inode *inode, size_t logical_block_index, uint32_t *physical_block, bool *created_data_block,
                                bool *created_indirect_block);

int directory_rollback_data_block(Disk *disk, Inode *dir_inode, size_t logical_block_index, uint32_t physical_block, bool created_data_block,
                                  bool created_indirect_block);

int directory_find_entry(Disk *disk, uint32_t dir_inode_number, const char *name, uint32_t *inode_number);
int directory_add_entry(Disk *disk, uint32_t dir_inode_number, const char *name, uint32_t inode_number);
int directory_remove_entry(Disk *disk, uint32_t dir_inode_number, const char *name);

#endif