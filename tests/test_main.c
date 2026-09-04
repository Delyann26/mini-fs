#include "test_bitmap.h"
#include "test_disk.h"
#include "test_filesystem.h"
#include "test_fs_alloc.h"
#include "test_inode.h"
#include "test_inode_table.h"
#include "test_superblock.h"
#include <stdio.h>

int main(void) {
    test_disk();
    test_superblock();
    test_bitmap();
    test_inode();
    test_inode_table();
    test_fs_alloc();
    test_filesystem();
    printf("All tests passed\n");
    return 0;
}