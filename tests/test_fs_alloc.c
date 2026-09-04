#include "test_fs_alloc.h"

#include "bitmap.h"
#include "common.h"
#include "disk.h"
#include "fs_alloc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_DISK_PATH "test_fs_alloc.img"

static void create_test_disk(Disk *disk) {
    assert(disk_init(disk) == FS_OK);
    assert(disk_create(disk, TEST_DISK_PATH) == FS_OK);
}

static void destroy_test_disk(Disk *disk) {
    assert(disk_close(disk) == FS_OK);
    remove(TEST_DISK_PATH);
}

static void reserve_root_inode(Disk *disk) {
    uint8_t bitmap[BLOCK_SIZE];

    assert(disk_read(disk, bitmap, INODE_BITMAP_BLOCK) == FS_OK);

    assert(bitmap_set_bit(bitmap, MAXIMUM_INODES, ROOT_INODE) == FS_OK);

    assert(disk_write(disk, bitmap, INODE_BITMAP_BLOCK) == FS_OK);
}

// INODE ALLOCATION

static void test_allocate_inode(void) {
    Disk disk;
    create_test_disk(&disk);
    reserve_root_inode(&disk);

    uint32_t inode_number;

    assert(fs_allocate_inode(&disk, &inode_number) == FS_OK);
    assert(inode_number == 1);

    uint8_t bitmap[BLOCK_SIZE];

    assert(disk_read(&disk, bitmap, INODE_BITMAP_BLOCK) == FS_OK);

    assert(!bitmap_is_free(bitmap, MAXIMUM_INODES, ROOT_INODE));
    assert(!bitmap_is_free(bitmap, MAXIMUM_INODES, inode_number));

    destroy_test_disk(&disk);
}

static void test_allocate_multiple_inodes(void) {
    Disk disk;
    create_test_disk(&disk);
    reserve_root_inode(&disk);

    uint32_t inode1;
    uint32_t inode2;
    uint32_t inode3;

    assert(fs_allocate_inode(&disk, &inode1) == FS_OK);
    assert(fs_allocate_inode(&disk, &inode2) == FS_OK);
    assert(fs_allocate_inode(&disk, &inode3) == FS_OK);

    assert(inode1 == 1);
    assert(inode2 == 2);
    assert(inode3 == 3);

    destroy_test_disk(&disk);
}

static void test_free_inode(void) {
    Disk disk;
    create_test_disk(&disk);
    reserve_root_inode(&disk);

    uint32_t inode_number;

    assert(fs_allocate_inode(&disk, &inode_number) == FS_OK);
    assert(!bitmap_is_free(NULL, MAXIMUM_INODES, inode_number));

    assert(fs_free_inode(&disk, inode_number) == FS_OK);

    uint8_t bitmap[BLOCK_SIZE];

    assert(disk_read(&disk, bitmap, INODE_BITMAP_BLOCK) == FS_OK);

    assert(bitmap_is_free(bitmap, MAXIMUM_INODES, inode_number));

    destroy_test_disk(&disk);
}

static void test_free_then_reallocate_inode(void) {
    Disk disk;
    create_test_disk(&disk);
    reserve_root_inode(&disk);

    uint32_t inode1;
    uint32_t inode2;
    uint32_t inode3;

    assert(fs_allocate_inode(&disk, &inode1) == FS_OK);
    assert(fs_allocate_inode(&disk, &inode2) == FS_OK);
    assert(fs_allocate_inode(&disk, &inode3) == FS_OK);

    assert(inode1 == 1);
    assert(inode2 == 2);
    assert(inode3 == 3);

    assert(fs_free_inode(&disk, inode2) == FS_OK);

    uint32_t new_inode;

    assert(fs_allocate_inode(&disk, &new_inode) == FS_OK);

    assert(new_inode == inode2);

    destroy_test_disk(&disk);
}

static void test_free_inode_twice(void) {
    Disk disk;
    create_test_disk(&disk);
    reserve_root_inode(&disk);

    uint32_t inode_number;

    assert(fs_allocate_inode(&disk, &inode_number) == FS_OK);

    assert(fs_free_inode(&disk, inode_number) == FS_OK);

    assert(fs_free_inode(&disk, inode_number) == FS_ERR_INODE_IS_ALREADY_FREE);

    destroy_test_disk(&disk);
}

static void test_cannot_free_root_inode(void) {
    Disk disk;
    create_test_disk(&disk);
    reserve_root_inode(&disk);

    assert(fs_free_inode(&disk, ROOT_INODE) == FS_ERR_PROTECTED_INODE);

    destroy_test_disk(&disk);
}

static void test_free_invalid_inode(void) {
    Disk disk;
    create_test_disk(&disk);
    reserve_root_inode(&disk);

    assert(fs_free_inode(&disk, MAXIMUM_INODES) == FS_ERR_INDEX_OUT_OF_BOUNDS);

    destroy_test_disk(&disk);
}

static void test_allocate_inode_when_full(void) {
    Disk disk;
    create_test_disk(&disk);

    uint8_t bitmap[BLOCK_SIZE];
    memset(bitmap, 0, sizeof(bitmap));

    for (size_t i = 0; i < MAXIMUM_INODES; i++) {
        assert(bitmap_set_bit(bitmap, MAXIMUM_INODES, i) == FS_OK);
    }

    assert(disk_write(&disk, bitmap, INODE_BITMAP_BLOCK) == FS_OK);

    uint32_t inode_number = 12345;

    assert(fs_allocate_inode(&disk, &inode_number) == FS_ERR_NO_FREE_INODES);

    /*
     * The output parameter should only be changed when allocation
     * succeeds.
     */
    assert(inode_number == 12345);

    destroy_test_disk(&disk);
}

static void test_allocate_inode_null_arguments(void) {
    Disk disk;
    create_test_disk(&disk);

    uint32_t inode_number;

    assert(fs_allocate_inode(NULL, &inode_number) == FS_ERR_NULL);
    assert(fs_allocate_inode(&disk, NULL) == FS_ERR_NULL);

    destroy_test_disk(&disk);
}

static void test_free_inode_null_disk(void) { assert(fs_free_inode(NULL, 1) == FS_ERR_NULL); }

// DATA BLOCK ALLOCATION

static void test_allocate_data_block(void) {
    Disk disk;
    create_test_disk(&disk);

    uint32_t block_number;

    assert(fs_allocate_data_block(&disk, &block_number) == FS_OK);

    assert(block_number == USER_DATA_BLOCK_START);

    uint8_t bitmap[BLOCK_SIZE];

    assert(disk_read(&disk, bitmap, DATA_BITMAP_BLOCK) == FS_OK);

    /*
     * Physical block USER_DATA_BLOCK_START corresponds to
     * bitmap index 0.
     */
    assert(!bitmap_is_free(bitmap, TOTAL_USER_DATA_BLOCKS, 0));

    destroy_test_disk(&disk);
}

static void test_allocate_multiple_data_blocks(void) {
    Disk disk;
    create_test_disk(&disk);

    uint32_t block1;
    uint32_t block2;
    uint32_t block3;

    assert(fs_allocate_data_block(&disk, &block1) == FS_OK);
    assert(fs_allocate_data_block(&disk, &block2) == FS_OK);
    assert(fs_allocate_data_block(&disk, &block3) == FS_OK);

    assert(block1 == USER_DATA_BLOCK_START);
    assert(block2 == USER_DATA_BLOCK_START + 1);
    assert(block3 == USER_DATA_BLOCK_START + 2);

    destroy_test_disk(&disk);
}

static void test_free_data_block(void) {
    Disk disk;
    create_test_disk(&disk);

    uint32_t block_number;

    assert(fs_allocate_data_block(&disk, &block_number) == FS_OK);

    assert(fs_free_data_block(&disk, block_number) == FS_OK);

    uint8_t bitmap[BLOCK_SIZE];

    assert(disk_read(&disk, bitmap, DATA_BITMAP_BLOCK) == FS_OK);

    size_t bitmap_index = block_number - USER_DATA_BLOCK_START;

    assert(bitmap_is_free(bitmap, TOTAL_USER_DATA_BLOCKS, bitmap_index));

    destroy_test_disk(&disk);
}

static void test_free_then_reallocate_data_block(void) {
    Disk disk;
    create_test_disk(&disk);

    uint32_t block1;
    uint32_t block2;
    uint32_t block3;

    assert(fs_allocate_data_block(&disk, &block1) == FS_OK);
    assert(fs_allocate_data_block(&disk, &block2) == FS_OK);
    assert(fs_allocate_data_block(&disk, &block3) == FS_OK);

    assert(block1 == USER_DATA_BLOCK_START);
    assert(block2 == USER_DATA_BLOCK_START + 1);
    assert(block3 == USER_DATA_BLOCK_START + 2);

    assert(fs_free_data_block(&disk, block2) == FS_OK);

    uint32_t new_block;

    assert(fs_allocate_data_block(&disk, &new_block) == FS_OK);

    assert(new_block == block2);

    destroy_test_disk(&disk);
}

static void test_free_data_block_twice(void) {
    Disk disk;
    create_test_disk(&disk);

    uint32_t block_number;

    assert(fs_allocate_data_block(&disk, &block_number) == FS_OK);

    assert(fs_free_data_block(&disk, block_number) == FS_OK);

    assert(fs_free_data_block(&disk, block_number) == FS_ERR_DATA_BLOCK_IS_ALREADY_FREE);

    destroy_test_disk(&disk);
}

static void test_cannot_free_protected_blocks(void) {
    Disk disk;
    create_test_disk(&disk);

    assert(fs_free_data_block(&disk, 0) == FS_ERR_PROTECTED_BLOCK);

    assert(fs_free_data_block(&disk, INODE_BITMAP_BLOCK) == FS_ERR_PROTECTED_BLOCK);

    assert(fs_free_data_block(&disk, USER_DATA_BLOCK_START - 1) == FS_ERR_PROTECTED_BLOCK);

    destroy_test_disk(&disk);
}

static void test_free_invalid_data_block(void) {
    Disk disk;
    create_test_disk(&disk);

    assert(fs_free_data_block(&disk, MAXIMUM_BLOCKS) == FS_ERR_INDEX_OUT_OF_BOUNDS);

    destroy_test_disk(&disk);
}

static void test_allocate_last_data_block(void) {
    Disk disk;
    create_test_disk(&disk);

    uint8_t bitmap[BLOCK_SIZE];
    memset(bitmap, 0, sizeof(bitmap));

    /*
     * Allocate every data-block bitmap bit except the final one.
     */
    for (size_t i = 0; i < TOTAL_USER_DATA_BLOCKS - 1; i++) {
        assert(bitmap_set_bit(bitmap, TOTAL_USER_DATA_BLOCKS, i) == FS_OK);
    }

    assert(disk_write(&disk, bitmap, DATA_BITMAP_BLOCK) == FS_OK);

    uint32_t block_number;

    assert(fs_allocate_data_block(&disk, &block_number) == FS_OK);

    /*
     * Last bitmap index:
     *
     * TOTAL_USER_DATA_BLOCKS - 1 = 55
     *
     * Physical block:
     *
     * USER_DATA_BLOCK_START + 55 = 63
     */
    assert(block_number == MAXIMUM_BLOCKS - 1);

    destroy_test_disk(&disk);
}

static void test_allocate_data_block_when_full(void) {
    Disk disk;
    create_test_disk(&disk);

    uint8_t bitmap[BLOCK_SIZE];
    memset(bitmap, 0, sizeof(bitmap));

    for (size_t i = 0; i < TOTAL_USER_DATA_BLOCKS; i++) {
        assert(bitmap_set_bit(bitmap, TOTAL_USER_DATA_BLOCKS, i) == FS_OK);
    }

    assert(disk_write(&disk, bitmap, DATA_BITMAP_BLOCK) == FS_OK);

    uint32_t block_number = 12345;

    assert(fs_allocate_data_block(&disk, &block_number) == FS_ERR_NO_FREE_DATA_BLOCKS);

    assert(block_number == 12345);

    destroy_test_disk(&disk);
}

static void test_allocate_data_block_null_arguments(void) {
    Disk disk;
    create_test_disk(&disk);

    uint32_t block_number;

    assert(fs_allocate_data_block(NULL, &block_number) == FS_ERR_NULL);

    assert(fs_allocate_data_block(&disk, NULL) == FS_ERR_NULL);

    destroy_test_disk(&disk);
}

static void test_free_data_block_null_disk(void) { assert(fs_free_data_block(NULL, USER_DATA_BLOCK_START) == FS_ERR_NULL); }

/* --------------------------------------------------------- */

void test_fs_alloc(void) {
    test_allocate_inode();
    test_allocate_multiple_inodes();
    test_free_inode();
    test_free_then_reallocate_inode();
    test_free_inode_twice();
    test_cannot_free_root_inode();
    test_free_invalid_inode();
    test_allocate_inode_when_full();
    test_allocate_inode_null_arguments();
    test_free_inode_null_disk();

    test_allocate_data_block();
    test_allocate_multiple_data_blocks();
    test_free_data_block();
    test_free_then_reallocate_data_block();
    test_free_data_block_twice();
    test_cannot_free_protected_blocks();
    test_free_invalid_data_block();
    test_allocate_last_data_block();
    test_allocate_data_block_when_full();
    test_allocate_data_block_null_arguments();
    test_free_data_block_null_disk();

    printf("FS Allocator: All tests passed!\n");
}