#pragma once

#include "stdint.h"
#include "stddef.h"

struct slab_list_node_t
{
    struct slab_list_node_t* next;
};

struct slab_page_desc_t
{
    uint32_t alloc_count;
    struct slab_page_desc_t* next;
    struct slab_page_desc_t* pre;
    struct slab_list_node_t slab[0];
};

struct slab_slot_desc_t
{
    struct slab_page_desc_t* free_page_head;
    struct slab_list_node_t* free_slab_head;
    uint32_t page_count;
    uint32_t free_slab_count;
    uint32_t slab_capacity;
} ;

void init_slab_module();

//assert(size % 16 == 0 && size > 0 && size <= 128);
void* tslab_malloc(size_t size);
void tslab_free(void* mem, size_t size);
void print_slab_status(size_t size);

#define SSIZEOF(type) ((sizeof(type) + 15) & 0xFFFFFFF0)
