#include "test_disk.h"
#include "common.h"
#include "disk.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_DISK_PATH "build/tests/test_disk.img"
#define INVALID_DISK_PATH "build/tests/invalid_disk.img"

static void test_disk_init(void) {
    Disk disk;
    assert(disk_init(NULL) == FS_ERR_NULL);
    assert(disk_init(&disk) == FS_OK);
    assert(disk.fd == -1);
}

static void test_disk_create(void) {
    Disk disk;
    assert(disk_init(&disk) == FS_OK);

    assert(disk_create(NULL, TEST_DISK_PATH) == FS_ERR_NULL);
    assert(disk_create(&disk, NULL) == FS_ERR_NULL);

    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);
    assert(disk.fd >= 0);

    assert(disk_create(&disk, TEST_DISK_PATH) == FS_ERR_ALREADY_OPEN);

    assert(disk_close(&disk) == FS_OK);

    int fd = open(TEST_DISK_PATH, O_RDONLY);
    assert(fd >= 0);

    off_t size = lseek(fd, 0, SEEK_END);
    assert(size == DISK_SIZE);

    assert(close(fd) == 0);
    unlink(TEST_DISK_PATH);
}

static void test_disk_open(void) {
    Disk disk;
    assert(disk_init(&disk) == FS_OK);

    assert(disk_open(NULL, TEST_DISK_PATH) == FS_ERR_NULL);
    assert(disk_open(&disk, NULL) == FS_ERR_NULL);

    assert(disk_open(&disk, "does_not_exist.img") == FS_ERR_OPEN);
    assert(disk.fd == -1);

    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);
    assert(disk_close(&disk) == FS_OK);

    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);
    assert(disk.fd >= 0);

    assert(disk_open(&disk, TEST_DISK_PATH) == FS_ERR_ALREADY_OPEN);
    assert(disk_close(&disk) == FS_OK);

    unlink(TEST_DISK_PATH);
}

static void test_invalid_disk_size(void) {
    int fd = open(INVALID_DISK_PATH, O_RDWR | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);

    uint8_t data[100];
    memset(data, 0, sizeof(data));

    assert(write(fd, data, sizeof(data)) == (ssize_t)sizeof(data));
    assert(close(fd) == 0);

    Disk disk;
    assert(disk_init(&disk) == FS_OK);

    assert(disk_open(&disk, INVALID_DISK_PATH) == FS_ERR_INVALID_DISK);
    assert(disk.fd == -1);

    unlink(INVALID_DISK_PATH);
}

static void test_disk_close(void) {
    Disk disk;

    assert(disk_close(NULL) == FS_ERR_NULL);

    assert(disk_init(&disk) == FS_OK);
    assert(disk_close(&disk) == FS_ERR_NOT_OPEN);

    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(disk_close(&disk) == FS_OK);
    assert(disk.fd == -1);

    assert(disk_close(&disk) == FS_ERR_NOT_OPEN);

    unlink(TEST_DISK_PATH);
}

static void test_disk_read_write() {
    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    uint8_t write_buffer[BLOCK_SIZE];
    uint8_t read_buffer[BLOCK_SIZE];

    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        write_buffer[i] = (uint8_t)(i % 256);
    }
    memset(read_buffer, 0, sizeof(read_buffer));

    size_t block = 10;
    assert(disk_write(&disk, write_buffer, block) == FS_OK);
    assert(disk_read(&disk, read_buffer, block) == FS_OK);
}

static void test_first_and_last_block(void) {
    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    uint8_t write_buffer[BLOCK_SIZE];
    uint8_t read_buffer[BLOCK_SIZE];

    memset(write_buffer, 0xAB, sizeof(write_buffer));

    assert(disk_write(&disk, write_buffer, 0) == FS_OK);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert(disk_read(&disk, read_buffer, 0) == FS_OK);

    assert(memcmp(write_buffer, read_buffer, BLOCK_SIZE) == 0);

    size_t last_block = MAXIMUM_BLOCKS - 1;

    memset(write_buffer, 0xCD, sizeof(write_buffer));

    assert(disk_write(&disk, write_buffer, last_block) == FS_OK);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert(disk_read(&disk, read_buffer, last_block) == FS_OK);

    assert(memcmp(write_buffer, read_buffer, BLOCK_SIZE) == 0);

    assert(disk_close(&disk) == FS_OK);

    unlink(TEST_DISK_PATH);
}

static void test_invalid_block(void) {
    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    uint8_t buffer[BLOCK_SIZE];

    size_t invalid_block = MAXIMUM_BLOCKS;

    assert(disk_read(&disk, buffer, invalid_block) == FS_ERR_INVALID_BLOCK);

    assert(disk_write(&disk, buffer, invalid_block) == FS_ERR_INVALID_BLOCK);

    assert(disk_close(&disk) == FS_OK);

    unlink(TEST_DISK_PATH);
}

static void test_null_buffers(void) {
    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(disk_read(&disk, NULL, 0) == FS_ERR_NULL);
    assert(disk_write(&disk, NULL, 0) == FS_ERR_NULL);

    assert(disk_read(NULL, NULL, 0) == FS_ERR_NULL);
    assert(disk_write(NULL, NULL, 0) == FS_ERR_NULL);

    assert(disk_close(&disk) == FS_OK);

    unlink(TEST_DISK_PATH);
}

static void test_read_write_closed_disk(void) {
    Disk disk;
    assert(disk_init(&disk) == FS_OK);

    uint8_t buffer[BLOCK_SIZE];

    assert(disk_read(&disk, buffer, 0) == FS_ERR_NOT_OPEN);
    assert(disk_write(&disk, buffer, 0) == FS_ERR_NOT_OPEN);
}

static void test_block_independence(void) {
    Disk disk;
    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    uint8_t block5[BLOCK_SIZE];
    uint8_t block6[BLOCK_SIZE];
    uint8_t read_buffer[BLOCK_SIZE];

    memset(block5, 0x11, sizeof(block5));
    memset(block6, 0x22, sizeof(block6));

    assert(disk_write(&disk, block5, 5) == FS_OK);
    assert(disk_write(&disk, block6, 6) == FS_OK);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert(disk_read(&disk, read_buffer, 5) == FS_OK);
    assert(memcmp(read_buffer, block5, BLOCK_SIZE) == 0);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert(disk_read(&disk, read_buffer, 6) == FS_OK);
    assert(memcmp(read_buffer, block6, BLOCK_SIZE) == 0);

    assert(disk_close(&disk) == FS_OK);

    unlink(TEST_DISK_PATH);
}

static void test_persistence(void) {
    Disk disk;

    uint8_t write_buffer[BLOCK_SIZE];
    uint8_t read_buffer[BLOCK_SIZE];

    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        write_buffer[i] = (uint8_t)((i * 7) % 256);
    }

    assert(disk_init(&disk) == FS_OK);
    assert(disk_create(&disk, TEST_DISK_PATH) == FS_OK);

    assert(disk_write(&disk, write_buffer, 10) == FS_OK);

    assert(disk_close(&disk) == FS_OK);

    assert(disk_init(&disk) == FS_OK);
    assert(disk_open(&disk, TEST_DISK_PATH) == FS_OK);

    memset(read_buffer, 0, sizeof(read_buffer));

    assert(disk_read(&disk, read_buffer, 10) == FS_OK);
    assert(memcmp(write_buffer, read_buffer, BLOCK_SIZE) == 0);

    assert(disk_close(&disk) == FS_OK);

    unlink(TEST_DISK_PATH);
}

void test_disk(void) {
    unlink(TEST_DISK_PATH);
    unlink(INVALID_DISK_PATH);

    test_disk_init();
    test_disk_create();
    test_disk_open();
    test_invalid_disk_size();
    test_disk_close();
    test_disk_read_write();
    test_first_and_last_block();
    test_invalid_block();
    test_null_buffers();
    test_read_write_closed_disk();
    test_block_independence();
    test_persistence();

    unlink(TEST_DISK_PATH);
    unlink(INVALID_DISK_PATH);

    printf("Disk: All tests passed\n");
}