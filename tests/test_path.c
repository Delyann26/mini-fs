#include "test_path.h"
#include "common.h"
#include "directory.h"
#include "disk.h"
#include "filesystem.h"
#include "fs_alloc.h"
#include "inode.h"
#include "inode_table.h"
#include "path.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_DISK_PATH "test_path.img"

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

static uint32_t create_test_file(Disk *disk, uint32_t parent_inode_number, const char *name) {
    uint32_t inode_number;
    assert(fs_allocate_inode(disk, &inode_number) == FS_OK);
    Inode inode;
    assert(inode_init(&inode) == FS_OK);
    inode.type = INODE_TYPE_FILE;
    assert(inode_table_write(disk, inode_number, &inode) == FS_OK);
    assert(directory_add_entry(disk, parent_inode_number, name, inode_number) == FS_OK);

    return inode_number;
}

static void test_resolve_path_root(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number = 999;
    assert(resolve_path(&disk, "/", &inode_number) == FS_OK);
    assert(inode_number == ROOT_INODE);
    close_test_disk(&disk);
}

static void test_resolve_path_single_directory(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t docs_inode = create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t inode_number;
    assert(resolve_path(&disk, "/docs", &inode_number) == FS_OK);
    assert(inode_number == docs_inode);
    close_test_disk(&disk);
}

static void test_resolve_path_file_in_root(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t file_inode = create_test_file(&disk, ROOT_INODE, "hello.txt");
    uint32_t inode_number;
    assert(resolve_path(&disk, "/hello.txt", &inode_number) == FS_OK);
    assert(inode_number == file_inode);
    close_test_disk(&disk);
}

static void test_resolve_path_nested_directory(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t docs_inode = create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t projects_inode = create_test_directory(&disk, docs_inode, "projects");
    uint32_t inode_number;
    assert(resolve_path(&disk, "/docs/projects", &inode_number) == FS_OK);
    assert(inode_number == projects_inode);
    close_test_disk(&disk);
}

static void test_resolve_path_nested_file(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t docs_inode = create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t projects_inode = create_test_directory(&disk, docs_inode, "projects");
    uint32_t file_inode = create_test_file(&disk, projects_inode, "main.c");
    uint32_t inode_number;

    assert(resolve_path(&disk, "/docs/projects/main.c", &inode_number) == FS_OK);
    assert(inode_number == file_inode);
    close_test_disk(&disk);
}

static void test_resolve_path_missing_first_component(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, "/missing/file.txt", &inode_number) == FS_ERR_INVALID_PATH);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_missing_middle_component(void) {
    Disk disk;
    open_formatted_disk(&disk);
    create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, "/docs/missing/file.txt", &inode_number) == FS_ERR_INVALID_PATH);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_missing_final_component(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t docs_inode = create_test_directory(&disk, ROOT_INODE, "docs");
    create_test_directory(&disk, docs_inode, "projects");
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, "/docs/projects/missing.txt", &inode_number) == FS_ERR_INVALID_PATH);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_file_as_intermediate_component(void) {
    Disk disk;
    open_formatted_disk(&disk);
    create_test_file(&disk, ROOT_INODE, "file.txt");
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, "/file.txt/child", &inode_number) == FS_ERR_ENTRY_IS_NOT_DIRECTORY);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_empty_path(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, "", &inode_number) == FS_ERR_INVALID_PATH);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_relative_path(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, "docs/file.txt", &inode_number) == FS_ERR_INVALID_PATH);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_double_slash(void) {
    Disk disk;
    open_formatted_disk(&disk);
    create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, "/docs//file.txt", &inode_number) == FS_ERR_INVALID_PATH);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_trailing_slash(void) {
    Disk disk;
    open_formatted_disk(&disk);
    create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, "/docs/", &inode_number) == FS_ERR_INVALID_PATH);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_maximum_component_length(void) {
    Disk disk;
    open_formatted_disk(&disk);
    char name[DIRECTORY_NAME_SIZE];
    memset(name, 'a', MAX_FILENAME_LENGTH);
    name[MAX_FILENAME_LENGTH] = '\0';
    uint32_t expected_inode = create_test_directory(&disk, ROOT_INODE, name);
    char path[DIRECTORY_NAME_SIZE + 1];
    path[0] = '/';
    memcpy(path + 1, name, MAX_FILENAME_LENGTH + 1);
    uint32_t inode_number;
    assert(resolve_path(&disk, path, &inode_number) == FS_OK);
    assert(inode_number == expected_inode);
    close_test_disk(&disk);
}

static void test_resolve_path_component_too_long(void) {
    Disk disk;
    open_formatted_disk(&disk);
    char path[MAX_FILENAME_LENGTH + 3];
    path[0] = '/';
    memset(path + 1, 'a', MAX_FILENAME_LENGTH + 1);
    path[MAX_FILENAME_LENGTH + 2] = '\0';
    uint32_t inode_number = 12345;
    assert(resolve_path(&disk, path, &inode_number) == FS_ERR_INVALID_PATH);
    assert(inode_number == 12345);
    close_test_disk(&disk);
}

static void test_resolve_path_null_arguments(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t inode_number;
    assert(resolve_path(NULL, "/", &inode_number) == FS_ERR_NULL);
    assert(resolve_path(&disk, NULL, &inode_number) == FS_ERR_NULL);
    assert(resolve_path(&disk, "/", NULL) == FS_ERR_NULL);
    close_test_disk(&disk);
}

static void test_resolve_parent_path_root_parent(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t parent_inode = 999;
    char name[DIRECTORY_NAME_SIZE];

    assert(resolve_parent_path(&disk, "/hello.txt", &parent_inode, name) == FS_OK);
    assert(parent_inode == ROOT_INODE);
    assert(strcmp(name, "hello.txt") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_nested(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t docs_inode = create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t parent_inode;
    char name[DIRECTORY_NAME_SIZE];

    assert(resolve_parent_path(&disk, "/docs/file.txt", &parent_inode, name) == FS_OK);
    assert(parent_inode == docs_inode);
    assert(strcmp(name, "file.txt") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_deeply_nested(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t docs_inode = create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t projects_inode = create_test_directory(&disk, docs_inode, "projects");
    uint32_t src_inode = create_test_directory(&disk, projects_inode, "src");
    uint32_t parent_inode;
    char name[DIRECTORY_NAME_SIZE];

    assert(resolve_parent_path(&disk, "/docs/projects/src/main.c", &parent_inode, name) == FS_OK);
    assert(parent_inode == src_inode);
    assert(strcmp(name, "main.c") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_final_entry_does_not_exist(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t docs_inode = create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t parent_inode;
    char name[DIRECTORY_NAME_SIZE];

    assert(resolve_parent_path(&disk, "/docs/new_file.txt", &parent_inode, name) == FS_OK);
    assert(parent_inode == docs_inode);
    assert(strcmp(name, "new_file.txt") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_existing_final_entry(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t docs_inode = create_test_directory(&disk, ROOT_INODE, "docs");
    create_test_file(&disk, docs_inode, "file.txt");
    uint32_t parent_inode;
    char name[DIRECTORY_NAME_SIZE];

    assert(resolve_parent_path(&disk, "/docs/file.txt", &parent_inode, name) == FS_OK);
    assert(parent_inode == docs_inode);
    assert(strcmp(name, "file.txt") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_missing_parent(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t parent_inode = 12345;
    char name[DIRECTORY_NAME_SIZE];
    strcpy(name, "unchanged");

    assert(resolve_parent_path(&disk, "/missing/file.txt", &parent_inode, name) == FS_ERR_INVALID_PATH);
    assert(parent_inode == 12345);
    assert(strcmp(name, "unchanged") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_file_as_parent(void) {
    Disk disk;
    open_formatted_disk(&disk);
    create_test_file(&disk, ROOT_INODE, "file.txt");
    uint32_t parent_inode = 12345;
    char name[DIRECTORY_NAME_SIZE];
    strcpy(name, "unchanged");

    assert(resolve_parent_path(&disk, "/file.txt/child", &parent_inode, name) == FS_ERR_ENTRY_IS_NOT_DIRECTORY);
    assert(parent_inode == 12345);
    assert(strcmp(name, "unchanged") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_empty_path(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t parent_inode = 12345;
    char name[DIRECTORY_NAME_SIZE];
    strcpy(name, "unchanged");

    assert(resolve_parent_path(&disk, "", &parent_inode, name) == FS_ERR_INVALID_PATH);
    assert(parent_inode == 12345);
    assert(strcmp(name, "unchanged") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_root(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t parent_inode = 12345;
    char name[DIRECTORY_NAME_SIZE];
    strcpy(name, "unchanged");

    assert(resolve_parent_path(&disk, "/", &parent_inode, name) == FS_ERR_INVALID_PATH);
    assert(parent_inode == 12345);
    assert(strcmp(name, "unchanged") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_relative_path(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t parent_inode = 12345;
    char name[DIRECTORY_NAME_SIZE];
    strcpy(name, "unchanged");
    assert(resolve_parent_path(&disk, "docs/file.txt", &parent_inode, name) == FS_ERR_INVALID_PATH);
    assert(parent_inode == 12345);
    assert(strcmp(name, "unchanged") == 0);
    close_test_disk(&disk);
}

static void test_resolve_parent_path_trailing_slash(void) {
    Disk disk;
    open_formatted_disk(&disk);
    create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t parent_inode = 12345;
    char name[DIRECTORY_NAME_SIZE];
    strcpy(name, "unchanged");

    assert(resolve_parent_path(&disk, "/docs/", &parent_inode, name) == FS_ERR_INVALID_PATH);
    assert(parent_inode == 12345);
    assert(strcmp(name, "unchanged") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_double_slash(void) {
    Disk disk;
    open_formatted_disk(&disk);
    create_test_directory(&disk, ROOT_INODE, "docs");
    uint32_t parent_inode = 12345;
    char name[DIRECTORY_NAME_SIZE];
    strcpy(name, "unchanged");

    assert(resolve_parent_path(&disk, "/docs//file.txt", &parent_inode, name) == FS_ERR_INVALID_PATH);
    assert(parent_inode == 12345);
    assert(strcmp(name, "unchanged") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_maximum_name(void) {
    Disk disk;
    open_formatted_disk(&disk);
    char filename[DIRECTORY_NAME_SIZE];
    memset(filename, 'a', MAX_FILENAME_LENGTH);
    filename[MAX_FILENAME_LENGTH] = '\0';
    char path[DIRECTORY_NAME_SIZE + 1];
    path[0] = '/';
    memcpy(path + 1, filename, MAX_FILENAME_LENGTH + 1);
    uint32_t parent_inode;
    char name[DIRECTORY_NAME_SIZE];

    assert(resolve_parent_path(&disk, path, &parent_inode, name) == FS_OK);
    assert(parent_inode == ROOT_INODE);
    assert(strcmp(name, filename) == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_name_too_long(void) {
    Disk disk;
    open_formatted_disk(&disk);
    char path[MAX_FILENAME_LENGTH + 3];
    path[0] = '/';
    memset(path + 1, 'a', MAX_FILENAME_LENGTH + 1);
    path[MAX_FILENAME_LENGTH + 2] = '\0';
    uint32_t parent_inode = 12345;
    char name[DIRECTORY_NAME_SIZE];
    strcpy(name, "unchanged");

    assert(resolve_parent_path(&disk, path, &parent_inode, name) == FS_ERR_INVALID_PATH);
    assert(parent_inode == 12345);
    assert(strcmp(name, "unchanged") == 0);

    close_test_disk(&disk);
}

static void test_resolve_parent_path_null_arguments(void) {
    Disk disk;
    open_formatted_disk(&disk);
    uint32_t parent_inode;
    char name[DIRECTORY_NAME_SIZE];

    assert(resolve_parent_path(NULL, "/file.txt", &parent_inode, name) == FS_ERR_NULL);
    assert(resolve_parent_path(&disk, NULL, &parent_inode, name) == FS_ERR_NULL);
    assert(resolve_parent_path(&disk, "/file.txt", NULL, name) == FS_ERR_NULL);
    assert(resolve_parent_path(&disk, "/file.txt", &parent_inode, NULL) == FS_ERR_NULL);

    close_test_disk(&disk);
}

void test_path(void) {
    test_resolve_path_root();
    test_resolve_path_single_directory();
    test_resolve_path_file_in_root();
    test_resolve_path_nested_directory();
    test_resolve_path_nested_file();
    test_resolve_path_missing_first_component();
    test_resolve_path_missing_middle_component();
    test_resolve_path_missing_final_component();
    test_resolve_path_file_as_intermediate_component();
    test_resolve_path_empty_path();
    test_resolve_path_relative_path();
    test_resolve_path_double_slash();
    test_resolve_path_trailing_slash();
    test_resolve_path_maximum_component_length();
    test_resolve_path_component_too_long();
    test_resolve_path_null_arguments();

    test_resolve_parent_path_root_parent();
    test_resolve_parent_path_nested();
    test_resolve_parent_path_deeply_nested();
    test_resolve_parent_path_final_entry_does_not_exist();
    test_resolve_parent_path_existing_final_entry();
    test_resolve_parent_path_missing_parent();
    test_resolve_parent_path_file_as_parent();
    test_resolve_parent_path_empty_path();
    test_resolve_parent_path_root();
    test_resolve_parent_path_relative_path();
    test_resolve_parent_path_trailing_slash();
    test_resolve_parent_path_double_slash();
    test_resolve_parent_path_maximum_name();
    test_resolve_parent_path_name_too_long();
    test_resolve_parent_path_null_arguments();

    printf("Path: All tests passed!\n");
}