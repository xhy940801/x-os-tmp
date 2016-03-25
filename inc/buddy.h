#pragma once

#include "stdint.h"
#include "stddef.h"

/**
 * Buddy memory alloc for kernel link-address
 */

struct buddy_list_node_t
{
    struct buddy_list_node_t* next;
    struct buddy_list_node_t* prev;
};

struct buddy_alloc_t
{
    uint32_t* map;
    struct buddy_list_node_t* list_start;
    struct buddy_list_node_t* list_head;
    uint32_t blank;
};

void init_buddy_module();

void* get_address_area(size_t n);
void free_address_area(void* p, size_t n);
