#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "disk.h"
#include "stdbool.h"
#include "superblock.h"

typedef struct FileSystem {
    Disk disk;
    SuperBlock superblock;
    bool mounted;
} FileSystem;

int filesystem_init(FileSystem *fs);

int filesystem_format(const char *path);
int filesystem_mount(FileSystem *fs, const char *path);
int filesystem_unmount(FileSystem *fs);

#endif