#pragma once

#include "stdint.h"

struct level_bitmap_t
{
    uint32_t* bitmaps;
    uint32_t max_level;
};
