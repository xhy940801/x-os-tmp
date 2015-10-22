#pragma once

#include "stdint.h"
#include "stddef.h"

#define MEM_START 0xc0000000

typedef struct mem_desc
{
    uint16_t active;
    uint16_t share;
    uint16_t flags;
    uint16_t unused;
    void* point;
} mem_desc_t;

enum
{
    MEM_FLAGS_P = 0x0001,
    MEM_FLAGS_K = 0x0002,
    MEM_FLAGS_L = 0x0004,
    MEM_FLAGS_D = 0x0008,
    MEM_FLAGS_T = 0x0010,
    MEM_FLAGS_S = 0x0020,
    MEM_FLAGS_C = 0x0040
};

void move_bios_mem();

void init_mem_desc();

void* kgetpersistedmem(size_t size);
void* kgetpersistedpage(size_t size);
