#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>

int bitmap_set(uint8_t *bitmap, uint32_t index);
int bitmap_clear(uint8_t *bitmap, uint32_t index);
uint32_t bitmap_find_free(const uint8_t *bitmap, uint32_t max_bits);
uint32_t bitmap_count_used(const uint8_t *bitmap, uint32_t max_bits);

#endif BITMAP_H