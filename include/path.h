#ifndef PATH_H
#define PATH_H
#include "disk.h"
#include <stdint.h>

int resolve_path(Disk *disk, const char *path, uint32_t *inode_number);
int resolve_parent_path(Disk *disk, const char *path, uint32_t *inode_number, char *name);

#endif