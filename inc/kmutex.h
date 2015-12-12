#pragma once

#include "circular_list.h"

struct kmutex_desc_t
{
    struct list_node_t head;
};

void kmutex_init(struct kmutex_desc_t* mutex);
int kmutex_lock(struct kmutex_desc_t* mutex, int timeout);
void kmutex_unlock(struct kmutex_desc_t* mutex);
