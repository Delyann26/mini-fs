#include "bitmap.h"

size_t bitmap_find_first_free_bit(const uint8_t *bitmap, size_t bit_count) {
    if (bitmap == NULL) {
        return SIZE_MAX;
    }
    uint8_t mask = 1;
    for (size_t i = 0; i < bit_count; i++) {
        if (i % 8 == 0) {
            mask = 1;
        }
        if ((bitmap[i / 8] & mask) == 0) {
            return i;
        }
        mask = mask << 1;
    }
    return SIZE_MAX;
}

bool bitmap_is_free(const uint8_t *bitmap, size_t bit_count, size_t index) {
    if (bitmap == NULL || index >= bit_count) {
        return false;
    }
    size_t byte_index = index / 8;
    size_t bit_index = index % 8;
    uint8_t mask = 1 << bit_index;
    uint8_t check = (bitmap[byte_index] & mask);
    return !check;
}

int bitmap_set_bit(uint8_t *bitmap, size_t bit_count, size_t index) {
    if (bitmap == NULL) {
        return FS_ERR_NULL;
    }
    if (index >= bit_count) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }
    size_t byte_index = index / 8;
    size_t bit_index = index % 8;
    bitmap[byte_index] |= (1 << bit_index);
    return FS_OK;
}

int bitmap_clear_bit(uint8_t *bitmap, size_t bit_count, size_t index) {
    if (bitmap == NULL) {
        return FS_ERR_NULL;
    }
    if (index >= bit_count) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }
    size_t byte_index = index / 8;
    size_t bit_index = index % 8;
    bitmap[byte_index] &= ~(1 << bit_index);
    return FS_OK;
}
