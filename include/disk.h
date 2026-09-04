#ifndef DISK_H
#define DISK_H

#include <stddef.h>

typedef struct Disk {
    int fd;
} Disk;

int disk_init(Disk *disk);
int disk_create(Disk *disk, const char *path);
int disk_open(Disk *disk, const char *path);
int disk_close(Disk *disk);

int disk_read(Disk *disk, void *buffer, size_t block_index);
int disk_write(Disk *disk, const void *buffer, size_t block_index);

#endif