#include "bitmap.h"
#include <stdlib.h>

int bitmap_set(uint8_t *bitmap, uint32_t index) {
    if (bitmap == NULL) {
        return -1;
    }
    uint32_t byte_index = index / 8;
    uint32_t bit_index = index % 8;
    uint8_t mask = 1 << bit_index;
    bitmap[byte_index] |= mask;
    return 0;
}

int bitmap_clear(uint8_t *bitmap, uint32_t index) {
    if (bitmap == NULL) {
        return -1;
    }
    uint32_t byte_index = index / 8;
    uint32_t bit_index = index % 8;
    uint8_t mask = 1 << bit_index;
    bitmap[byte_index] &= ~mask;
    return 0;
}

uint32_t bitmap_find_free(const uint8_t *bitmap, uint32_t max_bits) {
    if (bitmap == NULL || max_bits == 0) {
        return UINT32_MAX;
    }
    for (size_t i = 0; i < max_bits; i++) {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;
        uint8_t mask = 1 << bit_index;
        uint8_t bit = (bitmap[byte_index] & mask) >> bit_index;
        if (bit == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

uint32_t bitmap_count_used(const uint8_t *bitmap, uint32_t max_bits) {
    if (bitmap == NULL) {
        return UINT32_MAX;
    }
    uint32_t counter = 0;
    for (size_t i = 0; i < max_bits; i++) {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;
        uint8_t mask = 1 << bit_index;
        uint8_t bit = bitmap[byte_index] & mask;
        if (bit != 0) {
            counter++;
        }
    }
    return counter;
}