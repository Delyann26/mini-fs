#include "test_inode.h"

#include "common.h"
#include "inode.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static void test_inode_init_null(void) { assert(inode_init(NULL) == FS_ERR_NULL); }

static void test_inode_init_basic_fields(void) {
    Inode inode;

    assert(inode_init(&inode) == FS_OK);

    assert(inode.type == INODE_TYPE_FREE);
    assert(inode.size == 0);
    assert(inode.indirect_block == INVALID_BLOCK);
}

static void test_inode_init_direct_blocks(void) {
    Inode inode;

    assert(inode_init(&inode) == FS_OK);

    for (size_t i = 0; i < INODE_DIRECT_POINTERS_COUNT; i++) {
        assert(inode.direct_blocks[i] == INVALID_BLOCK);
    }
}

static void test_inode_reinitialization(void) {
    Inode inode;

    inode.type = INODE_TYPE_FILE;
    inode.size = 1234;
    inode.indirect_block = 42;

    for (size_t i = 0; i < INODE_DIRECT_POINTERS_COUNT; i++) {
        inode.direct_blocks[i] = 8 + (uint32_t)i;
    }

    assert(inode_init(&inode) == FS_OK);

    assert(inode.type == INODE_TYPE_FREE);
    assert(inode.size == 0);
    assert(inode.indirect_block == INVALID_BLOCK);

    for (size_t i = 0; i < INODE_DIRECT_POINTERS_COUNT; i++) {
        assert(inode.direct_blocks[i] == INVALID_BLOCK);
    }
}

void test_inode(void) {
    test_inode_init_null();
    test_inode_init_basic_fields();
    test_inode_init_direct_blocks();
    test_inode_reinitialization();

    printf("Inode: All tests passed\n");
}