#ifndef BITMAP_H
#define BITMAP_H
#include "common.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int bitmap_set_bit(uint8_t *bitmap, size_t bit_count, size_t index);
int bitmap_clear_bit(uint8_t *bitmap, size_t bit_count, size_t index);

size_t bitmap_find_first_free_bit(const uint8_t *bitmap, size_t bit_count);
bool bitmap_is_free(const uint8_t *bitmap, size_t bit_count, size_t index);

#endif