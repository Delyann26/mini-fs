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

int filesystem_create_file(FileSystem *fs, const char *path);
int filesystem_create_directory(FileSystem *fs, const char *path);

// Overwrites the contents of an existing file with the supplied bytes, starting from byte 0.
// Writing zero bytes empties the file
int filesystem_write_file(FileSystem *fs, const char *path, const void *data, size_t size);

#endif