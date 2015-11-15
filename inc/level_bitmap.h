#pragma once

#include "stdint.h"
#include "stddef.h"

struct level_bitmap_t
{
    uint32_t* bitmaps;
    size_t max_level;
};

int level_bitmap_bit_test(struct level_bitmap_t* map, size_t bit);
int level_bitmap_bit_set(struct level_bitmap_t* map, size_t bit);
int level_bitmap_bit_clear(struct level_bitmap_t* map, size_t bit);
ssize_t level_bitmap_get_min(struct level_bitmap_t* map);
ssize_t level_bitmap_get_max(struct level_bitmap_t* map);
void level_bitmap_cpy(struct level_bitmap_t* dst, struct level_bitmap_t* src, size_t count);
void level_bitmap_batch_set(struct level_bitmap_t* map, size_t start, size_t end);
