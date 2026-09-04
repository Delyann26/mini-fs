#include "disk.h"
#include "superblock.h"
#include <stdio.h>

int main(void) {
    Disk disk;
    disk_init(&disk);
    disk_create(&disk, "disk.img");

    SuperBlock sb;
    superblock_init(&sb);

    superblock_write(&sb, &disk);

    return 0;
}