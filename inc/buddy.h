#pragma once

#include "stdint.h"
#include "stddef.h"

/**
 * Buddy memory alloc for kernel link-address
 */

typedef struct buddy_list_node
{
    struct buddy_list_node* next;
    struct buddy_list_node* prev;
} buddy_list_node_t;

typedef struct
{
    uint32_t* map;
    buddy_list_node_t* list_start;
    buddy_list_node_t* list_head;
} buddy_alloc_t;

void init_buddy_alloc();

void* get_address_area(size_t n);
void free_address_page(void* p, size_t n);
