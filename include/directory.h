#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <common.h>
#include <disk.h>
#include <stdint.h>

typedef struct DirectoryEntry {
    uint32_t inode_number;
    char name[DIRECTORY_NAME_SIZE];
} DirectoryEntry;

int directory_find_entry(Disk *disk, uint32_t dir_inode_number, const char *name, uint32_t *inode_number);
int directory_add_entry(Disk *disk, uint32_t dir_inode_number, const char *name, uint32_t inode_number);
int directory_remove_entry(Disk *disk, uint32_t dir_inode_number, const char *name);

#endif