#include "test_inode_table.h"

#include "common.h"
#include "disk.h"
#include "inode.h"
#include "inode_table.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_DISK_PATH "test_inode_table.img"

static void test_inode_table_write_read(void) {
    Disk disk;
    Inode written;
    Inode read;

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(inode_init(&written) == FS_OK);

    written.type = INODE_TYPE_FILE;
    written.size = 1234;
    written.direct_blocks[0] = USER_DATA_BLOCK_START;

    uint32_t inode_number = 10;

    assert(inode_table_write(&disk, inode_number, &written) == FS_OK);
    assert(inode_table_read(&disk, inode_number, &read) == FS_OK);

    assert(read.type == written.type);
    assert(read.size == written.size);
    assert(read.direct_blocks[0] == written.direct_blocks[0]);
    assert(read.indirect_block == written.indirect_block);

    assert(disk_close(&disk) == FS_OK);
}

static void test_inode_table_block_boundaries(void) {
    Disk disk;

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    uint32_t inode_numbers[] = {0, 15, 16, 31, 32, 47, 48, 63, 64, MAXIMUM_INODES - 1};
    size_t count = sizeof(inode_numbers) / sizeof(inode_numbers[0]);

    for (size_t i = 0; i < count; i++) {
        Inode written;
        Inode read;

        assert(inode_init(&written) == FS_OK);

        written.type = INODE_TYPE_FILE;
        written.size = inode_numbers[i] + 100;

        assert(inode_table_write(&disk, inode_numbers[i], &written) == FS_OK);

        assert(inode_table_read(&disk, inode_numbers[i], &read) == FS_OK);

        assert(read.type == INODE_TYPE_FILE);
        assert(read.size == written.size);
    }

    assert(disk_close(&disk) == FS_OK);
}

static void test_inode_table_preserves_neighboring_inode(void) {
    Disk disk;

    Inode first;
    Inode second;
    Inode read_first;
    Inode read_second;

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(inode_init(&first) == FS_OK);
    assert(inode_init(&second) == FS_OK);

    first.type = INODE_TYPE_FILE;
    first.size = 111;

    second.type = INODE_TYPE_DIRECTORY;
    second.size = 222;

    assert(inode_table_write(&disk, 5, &first) == FS_OK);
    assert(inode_table_write(&disk, 6, &second) == FS_OK);

    assert(inode_table_read(&disk, 5, &read_first) == FS_OK);
    assert(inode_table_read(&disk, 6, &read_second) == FS_OK);

    assert(read_first.type == INODE_TYPE_FILE);
    assert(read_first.size == 111);

    assert(read_second.type == INODE_TYPE_DIRECTORY);
    assert(read_second.size == 222);

    assert(disk_close(&disk) == FS_OK);
}

static void test_inode_table_cross_block_preservation(void) {
    Disk disk;

    Inode inode15;
    Inode inode16;

    Inode read15;
    Inode read16;

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(inode_init(&inode15) == FS_OK);
    assert(inode_init(&inode16) == FS_OK);

    inode15.type = INODE_TYPE_FILE;
    inode15.size = 1500;

    inode16.type = INODE_TYPE_DIRECTORY;
    inode16.size = 1600;

    /*
     * inode 15 is the last inode in block 3.
     * inode 16 is the first inode in block 4.
     */
    assert(inode_table_write(&disk, 15, &inode15) == FS_OK);
    assert(inode_table_write(&disk, 16, &inode16) == FS_OK);

    assert(inode_table_read(&disk, 15, &read15) == FS_OK);
    assert(inode_table_read(&disk, 16, &read16) == FS_OK);

    assert(read15.type == INODE_TYPE_FILE);
    assert(read15.size == 1500);

    assert(read16.type == INODE_TYPE_DIRECTORY);
    assert(read16.size == 1600);

    assert(disk_close(&disk) == FS_OK);
}

static void test_inode_table_persistence(void) {
    Disk disk;

    Inode written;
    Inode read;

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(inode_init(&written) == FS_OK);

    written.type = INODE_TYPE_FILE;
    written.size = 4096;
    written.direct_blocks[0] = USER_DATA_BLOCK_START;
    written.direct_blocks[1] = USER_DATA_BLOCK_START + 1;

    assert(inode_table_write(&disk, 37, &written) == FS_OK);

    assert(disk_close(&disk) == FS_OK);

    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    assert(inode_table_read(&disk, 37, &read) == FS_OK);

    assert(read.type == written.type);
    assert(read.size == written.size);
    assert(read.direct_blocks[0] == written.direct_blocks[0]);
    assert(read.direct_blocks[1] == written.direct_blocks[1]);

    assert(disk_close(&disk) == FS_OK);
}

static void test_inode_table_invalid_arguments(void) {
    Disk disk;
    Inode inode;

    assert(disk_init(&disk) == FS_OK);
    assert(inode_init(&inode) == FS_OK);

    assert(inode_table_read(NULL, 0, &inode) == FS_ERR_NULL);
    assert(inode_table_read(&disk, 0, NULL) == FS_ERR_NULL);

    assert(inode_table_write(NULL, 0, &inode) == FS_ERR_NULL);
    assert(inode_table_write(&disk, 0, NULL) == FS_ERR_NULL);

    assert(inode_table_read(&disk, MAXIMUM_INODES, &inode) == FS_ERR_INDEX_OUT_OF_BOUNDS);

    assert(inode_table_write(&disk, MAXIMUM_INODES, &inode) == FS_ERR_INDEX_OUT_OF_BOUNDS);
}

static void test_inode_table_closed_disk(void) {
    Disk disk;
    Inode inode;

    assert(disk_init(&disk) == FS_OK);
    assert(inode_init(&inode) == FS_OK);

    assert(inode_table_read(&disk, 0, &inode) == FS_ERR_NOT_OPEN);
    assert(inode_table_write(&disk, 0, &inode) == FS_ERR_NOT_OPEN);
}

void test_inode_table(void) {
    test_inode_table_write_read();
    test_inode_table_block_boundaries();
    test_inode_table_preserves_neighboring_inode();
    test_inode_table_cross_block_preservation();
    test_inode_table_persistence();
    test_inode_table_invalid_arguments();
    test_inode_table_closed_disk();

    remove(TEST_DISK_PATH);

    printf("Inode Table: All tests passed\n");
}