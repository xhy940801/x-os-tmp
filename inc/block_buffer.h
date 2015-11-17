#pragma once

#include "types.h"

#include "rb_tree.h"
#include "circular_list.h"
#include "ksemaphore.h"

//32m cache
#define BLKBUFFER_BLKCOUNT  8192
#define BLKBUFFER_BLKSIZE   4096

enum BLKBUFFER_FLAGS
{
    BLKBUFFER_FLAG_SYNC     = 1,
    BLKBUFFER_FLAG_DIRTY    = 2
};

struct block_buffer_desc_t
{
    struct rb_tree_node_t rb_tree_node;
    struct list_node_t list_node;
    uint16_t main_driver;
    uint16_t sub_driver;
    void* buffer;
    long op_time;
    uint32_t used_count;
    uint32_t flags;
};

struct block_buffer_manager_info_t
{
    struct rb_tree_head_t rb_tree_head;
    struct ksemaphore_desc_t inlist_sem;
    struct list_node_t free_list_head;
    struct list_node_t sync_list_head;
    struct list_node_t dirty_list_head;
};

void block_buffer_module_init();
