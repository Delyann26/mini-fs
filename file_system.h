#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include "common.h"
#include "disk.h"
#include "superblock.h"

typedef struct {
    Disk disk;
    SuperBlock superblock;
    int mounted;
} FileSystem;

int filesystem_init(FileSystem *fs);

int filesystem_format(const char *path);
int filesystem_mount(FileSystem *fs, const char *path);
int filesystem_unmount(FileSystem *fs);

#endif FILE_SYSTEM_H