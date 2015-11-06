#include "level_bitmap.h"
#include "asm.h"

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
    return (map->bitmaps[n]) & (1 << npos);
}

int level_bitmap_bit_set(struct level_bitmap_t* map, size_t bit)
{
    uint32_t* p = map->bitmaps;
    for(size_t cur_pos = map->max_level * 5, i = map->max_level; cur_pos > 0; cur_pos -= 5, --i)
    {
        size_t npos = (bit >> cur_pos) & 31;
        *p |= (1 << npos);
        p += npos * level_jump_sizes[i] + 1;
    }
    size_t npos = bit & 31;
    int rs = (*p) & (1 << npos);
    *p |= (1 << npos);
    return rs;
}

int level_bitmap_bit_clear(struct level_bitmap_t* map, size_t bit)
{
    size_t _bit = bit;
    size_t npos = bit & 31;
    uint32_t* p = map->bitmaps;
    while(bit)
    {
        bit >>= 5;
        p += bit;
    }
    p += map->max_level;
    int rs = (*p) & (1 << npos);
    *p &= (~(1 << npos));

    bit = _bit >> 5;
    for(size_t i = 1; i <= map->max_level && *p == 0; ++i)
    {
        npos = bit & 31;
        p -= (npos * level_jump_sizes[i] + 1);
        *p &= (~(1 << npos));
        bit >>= 5;
    }
    return rs;
}

ssize_t level_bitmap_get_min(struct level_bitmap_t* map)
{
    uint32_t* p = map->bitmaps;
    if(*p == 0)
        return -1;
    ssize_t rs = 0;
    for(size_t i = map->max_level; i > 0; --i)
    {
        size_t npos = _bsf(*p);
        rs += npos;
        rs <<= 5;
        p += npos * level_jump_sizes[i] + 1;
    }
    size_t npos = _bsf(*p);
    rs += npos;
    return rs;
}

ssize_t level_bitmap_get_max(struct level_bitmap_t* map)
{
    uint32_t* p = map->bitmaps;
    if(*p == 0)
        return -1;
    ssize_t rs = 0;
    for(size_t i = map->max_level; i > 0; --i)
    {
        size_t npos = _bsr(*p);
        rs += npos;
        rs <<= 5;
        p += npos * level_jump_sizes[i] + 1;
    }
    size_t npos = _bsr(*p);
    rs += npos;
    return rs;
}

void level_bitmap_cpy(struct level_bitmap_t* dst, struct level_bitmap_t* src)
{

}

void level_bitmap_batch_set(struct level_bitmap_t* map, size_t start, size_t end)
{
}
