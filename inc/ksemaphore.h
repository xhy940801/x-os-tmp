#pragma once

#include "stddef.h"

#include "circular_list.h"

struct ksemaphore_node_t
{
    struct list_node_t node;
    size_t demand;
};

struct ksemaphore_desc_t
{
    struct list_node_t head;
    size_t surplus;
};

void ksemaphore_init(struct ksemaphore_desc_t* sem, size_t surplus);
void ksemaphore_up(struct ksemaphore_desc_t* sem, size_t num);
int ksemaphore_down(struct ksemaphore_desc_t* sem, size_t demand, int timeout);

/**
 * resource lock order:
 * 1.   virtual address
 * 2.   physical address
 */
