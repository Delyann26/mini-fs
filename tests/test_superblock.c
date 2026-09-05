#include "test_superblock.h"
#include "common.h"
#include "disk.h"
#include "superblock.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_DISK_PATH "test_superblock.img"

static void test_superblock_init(void) {
    SuperBlock sb;

    assert(superblock_init(NULL) == FS_ERR_NULL);

    assert(superblock_init(&sb) == FS_OK);

    assert(sb.magic == FS_MAGIC);
    assert(sb.block_size == BLOCK_SIZE);
    assert(sb.total_blocks == MAXIMUM_BLOCKS);

    assert(sb.inode_bitmap_block == INODE_BITMAP_BLOCK);
    assert(sb.data_bitmap_block == DATA_BITMAP_BLOCK);

    assert(sb.inode_table_block_start == INODE_TABLE_BLOCK_START);
    assert(sb.total_inode_table_blocks == TOTAL_INODE_TABLE_BLOCKS);
    assert(sb.maximum_inodes == MAXIMUM_INODES);

    assert(sb.user_data_block_start == USER_DATA_BLOCK_START);
    assert(sb.total_user_data_blocks == TOTAL_USER_DATA_BLOCKS);

    assert(sb.root_inode == ROOT_INODE);
}

static void test_superblock_is_valid(void) {
    SuperBlock sb;

    assert(superblock_is_valid(NULL) == false);

    assert(superblock_init(&sb) == FS_OK);
    assert(superblock_is_valid(&sb) == true);

    sb.magic = 0;
    assert(superblock_is_valid(&sb) == false);
}

static void test_superblock_write_read(void) {
    Disk disk;
    SuperBlock written;
    SuperBlock read;

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(superblock_init(&written) == FS_OK);

    assert(superblock_write(&written, &disk) == FS_OK);
    assert(superblock_read(&read, &disk) == FS_OK);

    assert(memcmp(&written, &read, sizeof(SuperBlock)) == 0);

    assert(disk_close(&disk) == FS_OK);
}

static void test_superblock_persistence(void) {
    Disk disk;
    SuperBlock written;
    SuperBlock read;

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(superblock_init(&written) == FS_OK);
    assert(superblock_write(&written, &disk) == FS_OK);

    assert(disk_close(&disk) == FS_OK);

    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    assert(superblock_read(&read, &disk) == FS_OK);

    assert(memcmp(&written, &read, sizeof(SuperBlock)) == 0);

    assert(disk_close(&disk) == FS_OK);
}

static void test_superblock_invalid_write(void) {
    Disk disk;
    SuperBlock sb;

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(superblock_init(&sb) == FS_OK);

    sb.magic = 0;

    assert(superblock_write(&sb, &disk) == FS_ERR_INVALID_DISK);

    assert(disk_close(&disk) == FS_OK);
}

static void test_superblock_corrupted_disk(void) {
    Disk disk;
    SuperBlock sb;

    uint8_t corrupted_block[BLOCK_SIZE];
    memset(corrupted_block, 0, sizeof(corrupted_block));

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(disk_write(&disk, corrupted_block, SUPERBLOCK_BLOCK) == FS_OK);

    assert(superblock_read(&sb, &disk) == FS_ERR_INVALID_DISK);

    assert(disk_close(&disk) == FS_OK);
}

static void test_superblock_null_arguments(void) {
    Disk disk;
    SuperBlock sb;

    assert(disk_init(&disk) == FS_OK);

    assert(superblock_write(NULL, &disk) == FS_ERR_NULL);
    assert(superblock_write(&sb, NULL) == FS_ERR_NULL);

    assert(superblock_read(NULL, &disk) == FS_ERR_NULL);
    assert(superblock_read(&sb, NULL) == FS_ERR_NULL);
}

static void test_superblock_closed_disk(void) {
    Disk disk;
    SuperBlock sb;

    assert(disk_init(&disk) == FS_OK);
    assert(superblock_init(&sb) == FS_OK);

    assert(superblock_write(&sb, &disk) == FS_ERR_NOT_OPEN);
    assert(superblock_read(&sb, &disk) == FS_ERR_NOT_OPEN);
}

void test_superblock(void) {
    test_superblock_init();
    test_superblock_is_valid();
    test_superblock_write_read();
    test_superblock_persistence();
    test_superblock_invalid_write();
    test_superblock_corrupted_disk();
    test_superblock_null_arguments();
    test_superblock_closed_disk();

    unlink(TEST_DISK_PATH);

    printf("Superblock: All tests passed\n");
}