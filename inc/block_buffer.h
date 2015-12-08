#pragma once

#include "stdint.h"

#include "blkbuffer_rb_tree.h"
#include "circular_list.h"
#include "ksemaphore.h"
#include "kcond.h"

//32m cache
#define BLKBUFFER_BLKCOUNT  8192
#define BLKBUFFER_BLKSIZE   4096

enum BLKBUFFER_FLAGS
{
    BLKBUFFER_FLAG_INQUEUE  = 1,
    BLKBUFFER_FLAG_UNSYNC   = 2,
    BLKBUFFER_FLAG_DIRTY    = 4,
    BLKBUFFER_FLAG_LOCK     = 16,
    BLKBUFFER_FLAG_SYNCING  = 32,
    BLKBUFFER_FLAG_FLUSHING = 64
};

struct block_buffer_desc_t
{
    struct rb_tree_node_t rb_tree_node;
    struct list_node_t list_node;
    union
    {
        struct
        {
            uint16_t sub_driver;
            uint16_t main_driver;
        };
        uint32_t driver_type;
    };
    size_t block_no;
    void* buffer;
    long op_time;
    uint32_t used_count;
    uint32_t flags;
    struct kcond_desc_t op_cond;
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
struct block_buffer_desc_t* get_block_buffer(uint16_t main_driver, uint16_t sub_driver, size_t block_no, int timeout);
void release_block_buffer(struct block_buffer_desc_t* blk);
