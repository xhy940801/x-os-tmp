#pragma once

#include "stddef.h"
#include "stdint.h"

#include "circular_list.h"

struct process_info_t;

struct sleep_desc_t
{
    struct list_node_t node;
    long wakeup_jiffies;
};

struct time_wheel_desc_t
{
    struct list_node_t head;
};

void wait_module_init();
void kwait(struct process_info_t* proc);
void kwakeup(struct process_info_t* proc);

void ksleep(struct process_info_t* proc, uint32_t tick);

size_t ready_processes(struct process_info_t* procs[], size_t max);

