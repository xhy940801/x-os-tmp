#pragma once

#include "stdint.h"
#include "stddef.h"

#define MEM_START 0xc0000000

struct mem_desc_t
{
    uint16_t active;
    uint16_t share;
    uint16_t flags;
    uint16_t unused;
    void* point;
};

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
void init_mem_module();

void* kgetpersistedmem(size_t size);
void* kgetpersistedpage(size_t size);

uint32_t get_free_mem_size();

//get 2 ^ n pages
void* get_pages(size_t n, uint16_t share, uint16_t flags);
void free_pages(void* p, size_t n);
void* get_one_page(uint16_t share, uint16_t flags, uint32_t* physical_addr);

void flush_page_table();

void init_paging_module();

struct process_info_t;
int mem_fork(struct process_info_t* dst, struct process_info_t* src);
