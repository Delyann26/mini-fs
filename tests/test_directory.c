#include "test_directory.h"

#include "common.h"
#include "directory.h"
#include "disk.h"
#include "filesystem.h"
#include "fs_alloc.h"
#include "inode.h"
#include "inode_table.h"
#include <assert.h>
#include <directory.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_DISK_PATH "test_directory.img"

static void cleanup_test_disk(void) { remove(TEST_DISK_PATH); }

static void open_formatted_disk(Disk *disk) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);
    assert(disk_init(disk) == FS_OK);
    assert(disk_open(disk, TEST_DISK_PATH) == FS_OK);
}

static void close_test_disk(Disk *disk) {
    assert(disk_close(disk) == FS_OK);
    cleanup_test_disk();
}

static DirectoryEntry make_entry(uint32_t inode_number, const char *name) {
    DirectoryEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.inode_number = inode_number;
    strcpy(entry.name, name);
    return entry;
}

static void write_entries_to_block(Disk *disk, uint32_t block_number, const DirectoryEntry *entries, size_t entries_count) {
    assert(entries_count <= DIRECTORY_ENTRIES_PER_BLOCK);
    uint8_t block[BLOCK_SIZE];
    memset(block, 0, sizeof(block));
    memcpy(block, entries, entries_count * sizeof(DirectoryEntry));
    assert(disk_write(disk, block, block_number) == FS_OK);
}

static void test_directory_find_entry_empty_directory(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number = 12345;
    assert(directory_find_entry(&disk, ROOT_INODE, "hello.txt", &inode_number) == FS_ERR_ENTRY_NOT_FOUND);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_directory_find_entry_one_entry(void) {
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);
    DirectoryEntry entry = make_entry(5, "hello.txt");

    write_entries_to_block(&disk, root.direct_blocks[0], &entry, 1);
    root.size = DIRECTORY_ENTRY_SIZE;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, "hello.txt", &inode_number) == FS_OK);
    assert(inode_number == 5);

    close_test_disk(&disk);
}

static void test_directory_find_entry_multiple_entries(void) {
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);
    DirectoryEntry entries[3];
    entries[0] = make_entry(1, "a.txt");
    entries[1] = make_entry(2, "b.txt");
    entries[2] = make_entry(3, "c.txt");

    write_entries_to_block(&disk, root.direct_blocks[0], entries, 3);
    root.size = 3 * DIRECTORY_ENTRY_SIZE;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number;

    assert(directory_find_entry(&disk, ROOT_INODE, "a.txt", &inode_number) == FS_OK);
    assert(inode_number == 1);
    assert(directory_find_entry(&disk, ROOT_INODE, "b.txt", &inode_number) == FS_OK);
    assert(inode_number == 2);
    assert(directory_find_entry(&disk, ROOT_INODE, "c.txt", &inode_number) == FS_OK);
    assert(inode_number == 3);

    close_test_disk(&disk);
}

static void test_directory_find_entry_not_found(void) {
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);
    DirectoryEntry entries[2];
    entries[0] = make_entry(1, "a.txt");
    entries[1] = make_entry(2, "b.txt");

    write_entries_to_block(&disk, root.direct_blocks[0], entries, 2);
    root.size = 2 * DIRECTORY_ENTRY_SIZE;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number = 999;
    assert(directory_find_entry(&disk, ROOT_INODE, "missing.txt", &inode_number) == FS_ERR_ENTRY_NOT_FOUND);
    assert(inode_number == 999);

    close_test_disk(&disk);
}

static void test_directory_find_entry_second_block(void) {
    if (INODE_DIRECT_POINTERS_COUNT < 2) {
        return;
    }
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t second_block;
    assert(fs_allocate_data_block(&disk, &second_block) == FS_OK);
    root.direct_blocks[1] = second_block;

    DirectoryEntry first_block_entries[DIRECTORY_ENTRIES_PER_BLOCK];
    for (size_t i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++) {
        char name[DIRECTORY_NAME_SIZE];
        snprintf(name, sizeof(name), "entry_%zu", i);
        first_block_entries[i] = make_entry((uint32_t)(i % MAXIMUM_INODES), name);
    }

    write_entries_to_block(&disk, root.direct_blocks[0], first_block_entries, DIRECTORY_ENTRIES_PER_BLOCK);
    DirectoryEntry second_entry = make_entry(42, "second_block.txt");
    write_entries_to_block(&disk, second_block, &second_entry, 1);
    root.size = (DIRECTORY_ENTRIES_PER_BLOCK + 1) * DIRECTORY_ENTRY_SIZE;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, "second_block.txt", &inode_number) == FS_OK);
    assert(inode_number == 42);

    close_test_disk(&disk);
}

static void test_directory_find_entry_indirect_block(void) {
    if (INODE_DIRECT_POINTERS_COUNT + 2 > TOTAL_USER_DATA_BLOCKS) {
        return;
    }
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);

    for (size_t block_index = 0; block_index < INODE_DIRECT_POINTERS_COUNT; block_index++) {
        if (block_index > 0) {
            uint32_t block_number;
            assert(fs_allocate_data_block(&disk, &block_number) == FS_OK);
            root.direct_blocks[block_index] = block_number;
        }
        DirectoryEntry entries[DIRECTORY_ENTRIES_PER_BLOCK];
        for (size_t i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++) {
            size_t global_index = block_index * DIRECTORY_ENTRIES_PER_BLOCK + i;
            char name[DIRECTORY_NAME_SIZE];
            snprintf(name, sizeof(name), "entry_%zu", global_index);
            entries[i] = make_entry((uint32_t)(global_index % MAXIMUM_INODES), name);
        }
        write_entries_to_block(&disk, root.direct_blocks[block_index], entries, DIRECTORY_ENTRIES_PER_BLOCK);
    }

    uint32_t indirect_block;
    assert(fs_allocate_data_block(&disk, &indirect_block) == FS_OK);
    root.indirect_block = indirect_block;
    uint32_t indirect_pointers[INDIRECT_POINTERS_PER_BLOCK];
    memset(indirect_pointers, 0, sizeof(indirect_pointers));
    uint32_t indirect_data_block;
    assert(fs_allocate_data_block(&disk, &indirect_data_block) == FS_OK);
    indirect_pointers[0] = indirect_data_block;
    assert(disk_write(&disk, indirect_pointers, indirect_block) == FS_OK);
    DirectoryEntry indirect_entry = make_entry(37, "indirect_target.txt");
    write_entries_to_block(&disk, indirect_data_block, &indirect_entry, 1);

    root.size = (INODE_DIRECT_POINTERS_COUNT * DIRECTORY_ENTRIES_PER_BLOCK + 1) * DIRECTORY_ENTRY_SIZE;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, "indirect_target.txt", &inode_number) == FS_OK);
    assert(inode_number == 37);
    close_test_disk(&disk);
}

static void test_directory_find_entry_null_arguments(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number;
    assert(directory_find_entry(NULL, ROOT_INODE, "file.txt", &inode_number) == FS_ERR_NULL);
    assert(directory_find_entry(&disk, ROOT_INODE, NULL, &inode_number) == FS_ERR_NULL);
    assert(directory_find_entry(&disk, ROOT_INODE, "file.txt", NULL) == FS_ERR_NULL);
    close_test_disk(&disk);
}

static void test_directory_find_entry_invalid_inode_number(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, MAXIMUM_INODES, "file.txt", &inode_number) == FS_ERR_INDEX_OUT_OF_BOUNDS);
    close_test_disk(&disk);
}

static void test_directory_find_entry_empty_name(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, "", &inode_number) == FS_ERR_INVALID_FILE_NAME);
    close_test_disk(&disk);
}

static void test_directory_find_entry_too_long_name(void) {
    Disk disk;
    open_formatted_disk(&disk);
    char name[MAX_FILENAME_LENGTH + 2];
    memset(name, 'a', MAX_FILENAME_LENGTH + 1);
    name[MAX_FILENAME_LENGTH + 1] = '\0';
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, name, &inode_number) == FS_ERR_INVALID_FILE_NAME);
    close_test_disk(&disk);
}

static void test_directory_find_entry_not_directory(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t file_inode_number;
    assert(fs_allocate_inode(&disk, &file_inode_number) == FS_OK);
    Inode file_inode;
    assert(inode_init(&file_inode) == FS_OK);
    file_inode.type = INODE_TYPE_FILE;
    assert(inode_table_write(&disk, file_inode_number, &file_inode) == FS_OK);
    uint32_t result;
    assert(directory_find_entry(&disk, file_inode_number, "anything", &result) == FS_ERR_ENTRY_IS_NOT_DIRECTORY);
    close_test_disk(&disk);
}

static void test_directory_find_entry_invalid_size(void) {
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);
    root.size = DIRECTORY_ENTRY_SIZE + 1;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, "file.txt", &inode_number) == FS_ERR_INVALID_DISK);
    close_test_disk(&disk);
}

static void test_directory_find_entry_corrupted_inode_number(void) {
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);
    DirectoryEntry entry = make_entry(MAXIMUM_INODES, "bad.txt");
    write_entries_to_block(&disk, root.direct_blocks[0], &entry, 1);
    root.size = DIRECTORY_ENTRY_SIZE;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, "bad.txt", &inode_number) == FS_ERR_INVALID_DISK);
    close_test_disk(&disk);
}

static void test_directory_find_entry_corrupted_name(void) {
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);
    DirectoryEntry entry;
    entry.inode_number = 1;
    memset(entry.name, 'x', DIRECTORY_NAME_SIZE);
    write_entries_to_block(&disk, root.direct_blocks[0], &entry, 1);
    root.size = DIRECTORY_ENTRY_SIZE;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, "anything", &inode_number) == FS_ERR_INVALID_DISK);
    close_test_disk(&disk);
}

static void test_directory_find_entry_invalid_block(void) {
    Disk disk;
    open_formatted_disk(&disk);
    Inode root;
    assert(inode_table_read(&disk, ROOT_INODE, &root) == FS_OK);
    root.size = DIRECTORY_ENTRY_SIZE;
    root.direct_blocks[0] = INVALID_BLOCK;
    assert(inode_table_write(&disk, ROOT_INODE, &root) == FS_OK);
    uint32_t inode_number;
    assert(directory_find_entry(&disk, ROOT_INODE, "file.txt", &inode_number) == FS_ERR_INVALID_BLOCK);
    close_test_disk(&disk);
}

void test_directory(void) {
    test_directory_find_entry_empty_directory();
    test_directory_find_entry_one_entry();
    test_directory_find_entry_multiple_entries();
    test_directory_find_entry_not_found();
    test_directory_find_entry_second_block();
    test_directory_find_entry_indirect_block();
    test_directory_find_entry_null_arguments();
    test_directory_find_entry_invalid_inode_number();
    test_directory_find_entry_empty_name();
    test_directory_find_entry_too_long_name();
    test_directory_find_entry_not_directory();
    test_directory_find_entry_invalid_size();
    test_directory_find_entry_corrupted_inode_number();
    test_directory_find_entry_corrupted_name();
    test_directory_find_entry_invalid_block();

    printf("Directory: All tests passed!\n");
}