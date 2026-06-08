#include "common.h"
#include "disk.h"
#include "superblock.h"

typedef struct {
    Disk disk;
    SuperBlock superblock;
    int mounted;
} FileSystem;
