#include "test_bitmap.h"
#include "bitmap.h"
#include "common.h"
#include <stdio.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static void test_bitmap_is_free(void) {
    uint8_t bitmap[2] = {0};

    assert(bitmap_is_free(bitmap, 16, 0) == true);
    assert(bitmap_is_free(bitmap, 16, 7) == true);
    assert(bitmap_is_free(bitmap, 16, 8) == true);
    assert(bitmap_is_free(bitmap, 16, 15) == true);

    assert(bitmap_is_free(NULL, 16, 0) == false);
    assert(bitmap_is_free(bitmap, 16, 16) == false);
}

static void test_bitmap_set_bit(void) {
    uint8_t bitmap[2] = {0};

    assert(bitmap_set_bit(bitmap, 16, 0) == FS_OK);
    assert(bitmap_is_free(bitmap, 16, 0) == false);

    assert(bitmap_set_bit(bitmap, 16, 7) == FS_OK);
    assert(bitmap_is_free(bitmap, 16, 7) == false);

    assert(bitmap_set_bit(bitmap, 16, 8) == FS_OK);
    assert(bitmap_is_free(bitmap, 16, 8) == false);

    assert(bitmap_set_bit(bitmap, 16, 15) == FS_OK);
    assert(bitmap_is_free(bitmap, 16, 15) == false);
}

static void test_bitmap_set_does_not_change_other_bits(void) {
    uint8_t bitmap[2] = {0};

    assert(bitmap_set_bit(bitmap, 16, 8) == FS_OK);

    assert(bitmap_is_free(bitmap, 16, 7) == true);
    assert(bitmap_is_free(bitmap, 16, 8) == false);
    assert(bitmap_is_free(bitmap, 16, 9) == true);
}

static void test_bitmap_set_invalid_arguments(void) {
    uint8_t bitmap[1] = {0};

    assert(bitmap_set_bit(NULL, 8, 0) == FS_ERR_NULL);

    assert(bitmap_set_bit(bitmap, 8, 8) == FS_ERR_INDEX_OUT_OF_BOUNDS);

    assert(bitmap_set_bit(bitmap, 8, 100) == FS_ERR_INDEX_OUT_OF_BOUNDS);
}

static void test_bitmap_clear_bit(void) {
    uint8_t bitmap[2];

    memset(bitmap, 0xFF, sizeof(bitmap));

    assert(bitmap_clear_bit(bitmap, 16, 0) == FS_OK);
    assert(bitmap_is_free(bitmap, 16, 0) == true);

    assert(bitmap_clear_bit(bitmap, 16, 7) == FS_OK);
    assert(bitmap_is_free(bitmap, 16, 7) == true);

    assert(bitmap_clear_bit(bitmap, 16, 8) == FS_OK);
    assert(bitmap_is_free(bitmap, 16, 8) == true);

    assert(bitmap_clear_bit(bitmap, 16, 15) == FS_OK);
    assert(bitmap_is_free(bitmap, 16, 15) == true);
}

static void test_bitmap_clear_does_not_change_other_bits(void) {
    uint8_t bitmap[2];

    memset(bitmap, 0xFF, sizeof(bitmap));

    assert(bitmap_clear_bit(bitmap, 16, 8) == FS_OK);

    assert(bitmap_is_free(bitmap, 16, 7) == false);
    assert(bitmap_is_free(bitmap, 16, 8) == true);
    assert(bitmap_is_free(bitmap, 16, 9) == false);
}

static void test_bitmap_clear_invalid_arguments(void) {
    uint8_t bitmap[1] = {0};

    assert(bitmap_clear_bit(NULL, 8, 0) == FS_ERR_NULL);

    assert(bitmap_clear_bit(bitmap, 8, 8) == FS_ERR_INDEX_OUT_OF_BOUNDS);
}

static void test_bitmap_find_first_free_bit(void) {
    uint8_t bitmap[2] = {0};

    assert(bitmap_find_first_free_bit(bitmap, 16) == 0);

    assert(bitmap_set_bit(bitmap, 16, 0) == FS_OK);
    assert(bitmap_find_first_free_bit(bitmap, 16) == 1);

    for (size_t i = 1; i < 8; i++) {
        assert(bitmap_set_bit(bitmap, 16, i) == FS_OK);
    }

    /*
     * Bits 0..7 are now occupied.
     * This checks the transition from byte 0 to byte 1.
     */
    assert(bitmap_find_first_free_bit(bitmap, 16) == 8);

    assert(bitmap_set_bit(bitmap, 16, 8) == FS_OK);
    assert(bitmap_find_first_free_bit(bitmap, 16) == 9);
}

static void test_bitmap_find_when_full(void) {
    uint8_t bitmap[2];

    memset(bitmap, 0xFF, sizeof(bitmap));

    assert(bitmap_find_first_free_bit(bitmap, 16) == SIZE_MAX);
}

static void test_bitmap_find_null(void) { assert(bitmap_find_first_free_bit(NULL, 16) == SIZE_MAX); }

static void test_bitmap_non_byte_aligned_size(void) {
    /*
     * There are two bytes physically available, but only
     * the first 10 bits belong to the bitmap.
     */
    uint8_t bitmap[2] = {0};

    for (size_t i = 0; i < 9; i++) {
        assert(bitmap_set_bit(bitmap, 10, i) == FS_OK);
    }

    assert(bitmap_find_first_free_bit(bitmap, 10) == 9);

    assert(bitmap_set_bit(bitmap, 10, 9) == FS_OK);

    /*
     * Bits 10..15 physically exist in bitmap[1], but they are
     * outside bit_count and must never be returned.
     */
    assert(bitmap_find_first_free_bit(bitmap, 10) == SIZE_MAX);
}

void test_bitmap(void) {
    test_bitmap_is_free();

    test_bitmap_set_bit();
    test_bitmap_set_does_not_change_other_bits();
    test_bitmap_set_invalid_arguments();

    test_bitmap_clear_bit();
    test_bitmap_clear_does_not_change_other_bits();
    test_bitmap_clear_invalid_arguments();

    test_bitmap_find_first_free_bit();
    test_bitmap_find_when_full();
    test_bitmap_find_null();

    test_bitmap_non_byte_aligned_size();

    printf("Bitmap: All tests passed\n");
}