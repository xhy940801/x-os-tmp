#include "level_biimap.h"

static const size_t level_jump_sizes[] = {
    0,
    ((32 << 0) - 1) / (32 - 1),
    ((32 << 5) - 1) / (32 - 1),
    ((32 << 10) - 1) / (32 - 1),
    ((32 << 15) - 1) / (32 - 1),
    ((32 << 20) - 1) / (32 - 1),
    ((32 << 25) - 1) / (32 - 1),
    ((32 << 30) - 1) / (32 - 1),
};

int level_bitmap_bit_test(struct level_bitmap_t* map, size_t bit)
{
    size_t npos = bit & 31;
    size_t n = 0;
    while(bit)
    {
        bit >>= 5;
        n += bit;
    }
    n += map->max_level;
    return (map->bitmap[n]) & (1 << npos);
}

int level_bitmap_bit_set(struct level_bitmap_t* map, size_t bit)
{
    size_t i;
    unsigned int* p = map->bitmaps;
    for(i = 0; i < map->max_level; ++i)
    {
        size_t npos = bit & 31;
        *p &= (1 << npos);
        p += npos * level_jump_sizes[map->max_level - i] + 1;
        bit >> 5;
    }
    size_t npos = bit &= 31;
    int rs = (*p) & (1 << npos);
    *p |= (1 << npos);
    return rs;
}

//TODO
int level_bitmap_bit_clear(struct level_bitmap_t* map, size_t bit)
{
    size_t npos = bit & 31;
    size_t n = 0;
    while(bit)
    {
        bit >>= 5;
        n += bit;
    }
    n += map->max_level;
    int rs = (map->bitmap[n]) & (1 << npos);
    map->bitmap[n] &= (~(1 << npos));
    return rs;
}

ssize_t level_bitmap_get_min(struct level_bitmap_t* map)
{

}
