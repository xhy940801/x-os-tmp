#pragma once

#include "circular_list.h"
#include "stdint.h"

struct kcond_desc_t
{
    struct list_node_t head;
};

void kcond_init(struct kcond_desc_t* cond);
int kcond_wait(struct kcond_desc_t* cond, uint32_t timeout);
unsigned int kcond_signal(struct kcond_desc_t* cond);
unsigned int kcond_broadcast(struct kcond_desc_t* cond);
