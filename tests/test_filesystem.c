#include "test_filesystem.h"

#include "bitmap.h"
#include "common.h"
#include "directory.h"
#include "disk.h"
#include "filesystem.h"
#include "fs_alloc.h"
#include "inode.h"
#include "inode_table.h"
#include "path.h"
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

static uint32_t create_test_directory(Disk *disk, uint32_t parent_inode_number, const char *name) {
    uint32_t inode_number;
    assert(fs_allocate_inode(disk, &inode_number) == FS_OK);

    uint32_t data_block;
    assert(fs_allocate_data_block(disk, &data_block) == FS_OK);

    uint8_t empty_block[BLOCK_SIZE];
    memset(empty_block, 0, sizeof(empty_block));
    assert(disk_write(disk, empty_block, data_block) == FS_OK);

    Inode inode;
    assert(inode_init(&inode) == FS_OK);
    inode.type = INODE_TYPE_DIRECTORY;
    inode.direct_blocks[0] = data_block;
    assert(inode_table_write(disk, inode_number, &inode) == FS_OK);
    assert(directory_add_entry(disk, parent_inode_number, name, inode_number) == FS_OK);
    return inode_number;
}

static void test_filesystem_create_file_root(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/hello.txt") == FS_OK);

    uint32_t inode_number;
    assert(resolve_path(&fs.disk, "/hello.txt", &inode_number) == FS_OK);
    assert(inode_number != ROOT_INODE);

    Inode inode;
    assert(inode_table_read(&fs.disk, inode_number, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_FILE);
    assert(inode.size == 0);
    for (size_t i = 0; i < INODE_DIRECT_POINTERS_COUNT; i++) {
        assert(inode.direct_blocks[i] == INVALID_BLOCK);
    }
    assert(inode.indirect_block == INVALID_BLOCK);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_file_directory_entry(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/hello.txt") == FS_OK);

    uint32_t inode_number;
    assert(directory_find_entry(&fs.disk, ROOT_INODE, "hello.txt", &inode_number) == FS_OK);

    Inode inode;
    assert(inode_table_read(&fs.disk, inode_number, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_FILE);
    assert(inode.size == 0);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_file_parent_size(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);

    Inode root_before;
    assert(inode_table_read(&fs.disk, ROOT_INODE, &root_before) == FS_OK);
    assert(filesystem_create_file(&fs, "/hello.txt") == FS_OK);

    Inode root_after;
    assert(inode_table_read(&fs.disk, ROOT_INODE, &root_after) == FS_OK);
    assert(root_after.size == root_before.size + DIRECTORY_ENTRY_SIZE);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_file_nested(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);

    uint32_t docs_inode = create_test_directory(&fs.disk, ROOT_INODE, "docs");
    assert(filesystem_create_file(&fs, "/docs/file.txt") == FS_OK);
    uint32_t file_inode;
    assert(directory_find_entry(&fs.disk, docs_inode, "file.txt", &file_inode) == FS_OK);
    Inode inode;
    assert(inode_table_read(&fs.disk, file_inode, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_FILE);
    assert(inode.size == 0);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_file_deeply_nested(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);

    uint32_t docs_inode = create_test_directory(&fs.disk, ROOT_INODE, "docs");
    uint32_t projects_inode = create_test_directory(&fs.disk, docs_inode, "projects");
    uint32_t src_inode = create_test_directory(&fs.disk, projects_inode, "src");
    assert(filesystem_create_file(&fs, "/docs/projects/src/main.c") == FS_OK);
    uint32_t file_inode;
    assert(directory_find_entry(&fs.disk, src_inode, "main.c", &file_inode) == FS_OK);
    Inode inode;
    assert(inode_table_read(&fs.disk, file_inode, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_FILE);
    assert(inode.size == 0);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_file_duplicate(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/file.txt") == FS_OK);
    assert(filesystem_create_file(&fs, "/file.txt") == FS_ERR_ENTRY_ALREADY_EXISTS);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_file_duplicate_preserves_inode(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/file.txt") == FS_OK);
    uint32_t first_inode;
    assert(resolve_path(&fs.disk, "/file.txt", &first_inode) == FS_OK);
    assert(filesystem_create_file(&fs, "/file.txt") == FS_ERR_ENTRY_ALREADY_EXISTS);

    uint32_t second_inode;
    assert(resolve_path(&fs.disk, "/file.txt", &second_inode) == FS_OK);
    assert(first_inode == second_inode);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_file_missing_parent(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/missing/file.txt") == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_file_parent_is_file(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/file.txt") == FS_OK);
    assert(filesystem_create_file(&fs, "/file.txt/child.txt") == FS_ERR_ENTRY_IS_NOT_DIRECTORY);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_file_invalid_paths(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "") == FS_ERR_INVALID_PATH);
    assert(filesystem_create_file(&fs, "/") == FS_ERR_INVALID_PATH);
    assert(filesystem_create_file(&fs, "file.txt") == FS_ERR_INVALID_PATH);
    assert(filesystem_create_file(&fs, "/file.txt/") == FS_ERR_INVALID_PATH);
    assert(filesystem_create_file(&fs, "/dir//file.txt") == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_file_null_arguments(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(NULL, "/file.txt") == FS_ERR_NULL);
    assert(filesystem_create_file(&fs, NULL) == FS_ERR_NULL);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_file_not_mounted(void) {
    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_create_file(&fs, "/file.txt") == FS_ERR_NOT_MOUNTED);
}

static void test_filesystem_create_multiple_files(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/a.txt") == FS_OK);
    assert(filesystem_create_file(&fs, "/b.txt") == FS_OK);
    assert(filesystem_create_file(&fs, "/c.txt") == FS_OK);

    uint32_t a_inode;
    uint32_t b_inode;
    uint32_t c_inode;

    assert(resolve_path(&fs.disk, "/a.txt", &a_inode) == FS_OK);
    assert(resolve_path(&fs.disk, "/b.txt", &b_inode) == FS_OK);
    assert(resolve_path(&fs.disk, "/c.txt", &c_inode) == FS_OK);

    assert(a_inode != b_inode);
    assert(a_inode != c_inode);
    assert(b_inode != c_inode);

    Inode root;
    assert(inode_table_read(&fs.disk, ROOT_INODE, &root) == FS_OK);
    assert(root.size == 3 * DIRECTORY_ENTRY_SIZE);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_directory_root(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);

    uint32_t inode_number;
    assert(resolve_path(&fs.disk, "/docs", &inode_number) == FS_OK);
    assert(inode_number != ROOT_INODE);

    Inode inode;
    assert(inode_table_read(&fs.disk, inode_number, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_DIRECTORY);
    assert(inode.size == 0);

    assert(inode.direct_blocks[0] != INVALID_BLOCK);

    for (size_t i = 1; i < INODE_DIRECT_POINTERS_COUNT; i++) {
        assert(inode.direct_blocks[i] == INVALID_BLOCK);
    }
    assert(inode.indirect_block == INVALID_BLOCK);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_entry_in_parent(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);

    uint32_t entry_inode_number;
    assert(directory_find_entry(&fs.disk, ROOT_INODE, "docs", &entry_inode_number) == FS_OK);

    Inode inode;
    assert(inode_table_read(&fs.disk, entry_inode_number, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_DIRECTORY);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_parent_size(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);

    Inode root_before;
    assert(inode_table_read(&fs.disk, ROOT_INODE, &root_before) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);

    Inode root_after;
    assert(inode_table_read(&fs.disk, ROOT_INODE, &root_after) == FS_OK);
    assert(root_after.size == root_before.size + DIRECTORY_ENTRY_SIZE);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_first_block_zeroed(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);

    uint32_t inode_number;
    assert(resolve_path(&fs.disk, "/docs", &inode_number) == FS_OK);

    Inode inode;
    assert(inode_table_read(&fs.disk, inode_number, &inode) == FS_OK);
    assert(inode.direct_blocks[0] != INVALID_BLOCK);

    uint8_t block[BLOCK_SIZE];
    assert(disk_read(&fs.disk, block, inode.direct_blocks[0]) == FS_OK);
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        assert(block[i] == 0);
    }
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_nested(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs/projects") == FS_OK);

    uint32_t docs_inode;
    uint32_t projects_inode;
    assert(resolve_path(&fs.disk, "/docs", &docs_inode) == FS_OK);
    assert(resolve_path(&fs.disk, "/docs/projects", &projects_inode) == FS_OK);
    assert(docs_inode != projects_inode);

    Inode projects;
    assert(inode_table_read(&fs.disk, projects_inode, &projects) == FS_OK);
    assert(projects.type == INODE_TYPE_DIRECTORY);
    assert(projects.size == 0);
    assert(projects.direct_blocks[0] != INVALID_BLOCK);

    Inode docs;
    assert(inode_table_read(&fs.disk, docs_inode, &docs) == FS_OK);
    assert(docs.size == DIRECTORY_ENTRY_SIZE);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_deeply_nested(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs/projects") == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs/projects/src") == FS_OK);

    uint32_t inode_number;
    assert(resolve_path(&fs.disk, "/docs/projects/src", &inode_number) == FS_OK);

    Inode inode;
    assert(inode_table_read(&fs.disk, inode_number, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_DIRECTORY);
    assert(inode.size == 0);
    assert(inode.direct_blocks[0] != INVALID_BLOCK);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_multiple_directories(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);
    assert(filesystem_create_directory(&fs, "/projects") == FS_OK);
    assert(filesystem_create_directory(&fs, "/music") == FS_OK);

    uint32_t docs_inode;
    uint32_t projects_inode;
    uint32_t music_inode;
    assert(resolve_path(&fs.disk, "/docs", &docs_inode) == FS_OK);
    assert(resolve_path(&fs.disk, "/projects", &projects_inode) == FS_OK);
    assert(resolve_path(&fs.disk, "/music", &music_inode) == FS_OK);
    assert(docs_inode != projects_inode);
    assert(docs_inode != music_inode);
    assert(projects_inode != music_inode);

    Inode docs;
    Inode projects;
    Inode music;
    assert(inode_table_read(&fs.disk, docs_inode, &docs) == FS_OK);
    assert(inode_table_read(&fs.disk, projects_inode, &projects) == FS_OK);
    assert(inode_table_read(&fs.disk, music_inode, &music) == FS_OK);
    assert(docs.direct_blocks[0] != projects.direct_blocks[0]);
    assert(docs.direct_blocks[0] != music.direct_blocks[0]);
    assert(projects.direct_blocks[0] != music.direct_blocks[0]);

    Inode root;
    assert(inode_table_read(&fs.disk, ROOT_INODE, &root) == FS_OK);
    assert(root.size == 3 * DIRECTORY_ENTRY_SIZE);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_duplicate(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_ERR_ENTRY_ALREADY_EXISTS);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_duplicate_preserves_inode(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);

    uint32_t first_inode;
    assert(resolve_path(&fs.disk, "/docs", &first_inode) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_ERR_ENTRY_ALREADY_EXISTS);

    uint32_t second_inode;
    assert(resolve_path(&fs.disk, "/docs", &second_inode) == FS_OK);
    assert(first_inode == second_inode);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_name_already_file(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;

    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/test") == FS_OK);
    assert(filesystem_create_directory(&fs, "/test") == FS_ERR_ENTRY_ALREADY_EXISTS);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_directory_missing_parent(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/missing/projects") == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_directory_parent_is_file(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_file(&fs, "/file.txt") == FS_OK);
    assert(filesystem_create_directory(&fs, "/file.txt/child") == FS_ERR_ENTRY_IS_NOT_DIRECTORY);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_empty_path(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "") == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_directory_root_path(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);
    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/") == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_relative_path(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);
    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "docs") == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_trailing_slash(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);
    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs/") == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_double_slash(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs//projects") == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_directory_maximum_name(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    char directory_name[DIRECTORY_NAME_SIZE];
    memset(directory_name, 'a', MAX_FILENAME_LENGTH);
    directory_name[MAX_FILENAME_LENGTH] = '\0';
    char path[DIRECTORY_NAME_SIZE + 1];
    path[0] = '/';
    memcpy(path + 1, directory_name, MAX_FILENAME_LENGTH + 1);
    assert(filesystem_create_directory(&fs, path) == FS_OK);

    uint32_t inode_number;
    assert(resolve_path(&fs.disk, path, &inode_number) == FS_OK);
    Inode inode;
    assert(inode_table_read(&fs.disk, inode_number, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_DIRECTORY);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
}

static void test_filesystem_create_directory_name_too_long(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    char path[MAX_FILENAME_LENGTH + 3];
    path[0] = '/';
    memset(path + 1, 'a', MAX_FILENAME_LENGTH + 1);
    path[MAX_FILENAME_LENGTH + 2] = '\0';
    assert(filesystem_create_directory(&fs, path) == FS_ERR_INVALID_PATH);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_null_arguments(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);
    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(NULL, "/docs") == FS_ERR_NULL);
    assert(filesystem_create_directory(&fs, NULL) == FS_ERR_NULL);
    assert(filesystem_unmount(&fs) == FS_OK);
    cleanup_test_disk();
}

static void test_filesystem_create_directory_not_mounted(void) {
    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_ERR_NOT_MOUNTED);
}

static void test_filesystem_create_directory_persists_after_remount(void) {
    cleanup_test_disk();
    assert(filesystem_format(TEST_DISK_PATH) == FS_OK);

    FileSystem fs;
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs") == FS_OK);
    assert(filesystem_create_directory(&fs, "/docs/projects") == FS_OK);
    assert(filesystem_unmount(&fs) == FS_OK);
    assert(filesystem_init(&fs) == FS_OK);
    assert(filesystem_mount(&fs, TEST_DISK_PATH) == FS_OK);

    uint32_t inode_number;
    assert(resolve_path(&fs.disk, "/docs/projects", &inode_number) == FS_OK);

    Inode inode;
    assert(inode_table_read(&fs.disk, inode_number, &inode) == FS_OK);
    assert(inode.type == INODE_TYPE_DIRECTORY);
    assert(inode.direct_blocks[0] != INVALID_BLOCK);
    assert(filesystem_unmount(&fs) == FS_OK);

    cleanup_test_disk();
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

    test_filesystem_create_file_root();
    test_filesystem_create_file_directory_entry();
    test_filesystem_create_file_parent_size();
    test_filesystem_create_file_nested();
    test_filesystem_create_file_deeply_nested();
    test_filesystem_create_file_duplicate();
    test_filesystem_create_file_duplicate_preserves_inode();
    test_filesystem_create_file_missing_parent();
    test_filesystem_create_file_parent_is_file();
    test_filesystem_create_file_invalid_paths();
    test_filesystem_create_file_null_arguments();
    test_filesystem_create_file_not_mounted();
    test_filesystem_create_multiple_files();

    test_filesystem_create_directory_root();
    test_filesystem_create_directory_entry_in_parent();
    test_filesystem_create_directory_parent_size();
    test_filesystem_create_directory_first_block_zeroed();
    test_filesystem_create_directory_nested();
    test_filesystem_create_directory_deeply_nested();
    test_filesystem_create_multiple_directories();
    test_filesystem_create_directory_duplicate();
    test_filesystem_create_directory_duplicate_preserves_inode();
    test_filesystem_create_directory_name_already_file();
    test_filesystem_create_directory_missing_parent();
    test_filesystem_create_directory_parent_is_file();
    test_filesystem_create_directory_empty_path();
    test_filesystem_create_directory_root_path();
    test_filesystem_create_directory_relative_path();
    test_filesystem_create_directory_trailing_slash();
    test_filesystem_create_directory_double_slash();
    test_filesystem_create_directory_maximum_name();
    test_filesystem_create_directory_name_too_long();
    test_filesystem_create_directory_null_arguments();
    test_filesystem_create_directory_not_mounted();
    test_filesystem_create_directory_persists_after_remount();

    printf("Filesystem: All tests passed!\n");
}