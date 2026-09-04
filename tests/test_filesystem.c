#include "test_filesystem.h"

#include "bitmap.h"
#include "common.h"
#include "disk.h"
#include "filesystem.h"
#include "inode.h"
#include "inode_table.h"
#include "superblock.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_DISK_PATH "test_filesystem.img"

static void cleanup_test_disk(void) { unlink(TEST_DISK_PATH); }

static void test_filesystem_format_null_path(void) { assert(filesystem_format(NULL) == FS_ERR_NULL); }

static void test_filesystem_format_creates_valid_disk(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    assert(disk_close(&disk) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_format_superblock(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    SuperBlock superblock;

    assert(superblock_read(&superblock, &disk) == FS_OK);
    assert(superblock_is_valid(&superblock));

    assert(disk_close(&disk) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_format_inode_bitmap(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    uint8_t inode_bitmap[BLOCK_SIZE];

    assert(disk_read(&disk, inode_bitmap, INODE_BITMAP_BLOCK) == FS_OK);

    /*
     * Root inode must be allocated.
     */
    assert(!bitmap_is_free(inode_bitmap, MAXIMUM_INODES, ROOT_INODE));

    /*
     * Every other inode must still be free.
     */
    for (size_t i = ROOT_INODE + 1; i < MAXIMUM_INODES; i++) {
        assert(bitmap_is_free(inode_bitmap, MAXIMUM_INODES, i));
    }

    assert(disk_close(&disk) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_format_data_bitmap(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    uint8_t data_bitmap[BLOCK_SIZE];

    assert(disk_read(&disk, data_bitmap, DATA_BITMAP_BLOCK) == FS_OK);

    /*
     * Bitmap index 0 corresponds to physical block
     * USER_DATA_BLOCK_START.
     *
     * It belongs to the root directory.
     */
    assert(!bitmap_is_free(data_bitmap, TOTAL_USER_DATA_BLOCKS, 0));

    /*
     * Every remaining user-data block must be free.
     */
    for (size_t i = 1; i < TOTAL_USER_DATA_BLOCKS; i++) {
        assert(bitmap_is_free(data_bitmap, TOTAL_USER_DATA_BLOCKS, i));
    }

    assert(disk_close(&disk) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_format_root_inode(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    Inode root_inode;

    assert(inode_table_read(&disk, ROOT_INODE, &root_inode) == FS_OK);

    assert(root_inode.type == INODE_TYPE_DIRECTORY);
    assert(root_inode.size == 0);

    /*
     * The first allocated data block should be physical
     * block USER_DATA_BLOCK_START.
     */
    assert(root_inode.direct_blocks[0] == USER_DATA_BLOCK_START);

    /*
     * inode_init() should have left all remaining
     * block pointers invalid.
     */
    for (size_t i = 1; i < INODE_DIRECT_POINTERS_COUNT; i++) {
        assert(root_inode.direct_blocks[i] == INVALID_BLOCK);
    }

    assert(root_inode.indirect_block == INVALID_BLOCK);

    assert(disk_close(&disk) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_format_root_data_block(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    uint8_t block[BLOCK_SIZE];

    assert(disk_read(&disk, block, USER_DATA_BLOCK_START) == FS_OK);

    /*
     * disk_create() zeroes the complete image and the
     * root directory is initially empty, so its data
     * block should still contain only zero bytes.
     */
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        assert(block[i] == 0);
    }

    assert(disk_close(&disk) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_format_twice(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    SuperBlock superblock;
    assert(superblock_read(&superblock, &disk) == FS_OK);
    assert(superblock_is_valid(&superblock));

    Inode root_inode;
    assert(inode_table_read(&disk, ROOT_INODE, &root_inode) == FS_OK);

    assert(root_inode.type == INODE_TYPE_DIRECTORY);
    assert(root_inode.direct_blocks[0] == USER_DATA_BLOCK_START);

    assert(disk_close(&disk) == FS_OK);

    cleanup_test_disk();
}

/////////

static void test_filesystem_mount(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);

    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);

    assert(fs.mounted);
    assert(fs.disk.fd != -1);
    assert(superblock_is_valid(&fs.superblock));

    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_mount_null_arguments(void) {
    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);

    assert(filesystem_mount(NULL, TEST_DISK_PATH) == FS_ERR_NULL);
    assert(filesystem_mount(&fs, NULL) == FS_ERR_NULL);
}

static void test_filesystem_mount_nonexistent_disk(void) {
    cleanup_test_disk();

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);

    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_ERR_OPEN);

    assert(!fs.mounted);
    assert(fs.disk.fd == -1);
}

static void test_filesystem_mount_invalid_disk(void) {
    cleanup_test_disk();

    /*
     * Create a correctly-sized disk image, but do not format it.
     * Its superblock is therefore all zeros and invalid.
     */
    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);
    assert(disk_close(&disk) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);

    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_ERR_INVALID_DISK);

    /*
     * Failed mount must restore the unmounted state.
     */
    assert(!fs.mounted);
    assert(fs.disk.fd == -1);

    SuperBlock empty = {0};

    assert(memcmp(&fs.superblock, &empty, sizeof(SuperBlock)) == 0);

    cleanup_test_disk();
}

static void test_filesystem_mount_already_mounted(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);

    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);

    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_ERR_ALREADY_MOUNTED);

    /*
     * First mount must still be intact.
     */
    assert(fs.mounted);
    assert(fs.disk.fd != -1);
    assert(superblock_is_valid(&fs.superblock));

    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

///

static void test_filesystem_unmount(void) {
    cleanup_test_disk();

    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);

    assert(filesystem_unmount(&fs) == FS_OK);

    assert(!fs.mounted);
    assert(fs.disk.fd == -1);

    SuperBlock empty = {0};

    assert(memcmp(&fs.superblock, &empty, sizeof(SuperBlock)) == 0);

    cleanup_test_disk();
}

static void test_filesystem_unmount_null(void) { assert(filesystem_unmount(NULL) == FS_ERR_NULL); }

static void test_filesystem_unmount_when_not_mounted(void) {
    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);

    assert(filesystem_unmount(&fs) == FS_ERR_NOT_MOUNTED);

    assert(!fs.mounted);
    assert(fs.disk.fd == -1);
}

void test_filesystem(void) {
    test_filesystem_format_null_path();
    test_filesystem_format_creates_valid_disk();
    test_filesystem_format_superblock();
    test_filesystem_format_inode_bitmap();
    test_filesystem_format_data_bitmap();
    test_filesystem_format_root_inode();
    test_filesystem_format_root_data_block();
    test_filesystem_format_twice();

    test_filesystem_mount();
    test_filesystem_mount_null_arguments();
    test_filesystem_mount_nonexistent_disk();
    test_filesystem_mount_invalid_disk();
    test_filesystem_mount_already_mounted();

    test_filesystem_unmount();
    test_filesystem_unmount_null();
    test_filesystem_unmount_when_not_mounted();

    printf("Filesystem: All tests passed!\n");
}