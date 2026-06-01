#ifndef DISK_H
#define DISK_H

#include "common.h"
#include <stddef.h>

typedef struct {
    int fd;
} Disk;

int disk_init(Disk *disk);

int disk_create(Disk *disk, const char *path); // used for formatting
int disk_open(Disk *disk, const char *path);   // used for mounting
int disk_close(Disk *disk);                    // after close set to -1
int disk_read_block(Disk *disk, size_t block_index, void *buffer);
int disk_write_block(Disk *disk, size_t block_index, const void *buffer);

#endif