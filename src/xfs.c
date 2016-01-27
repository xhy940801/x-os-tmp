#pragma once

#include "stdint.h"

#include "block_buffer.h"
#include "vfs.h"
#include "sched.h"
#include "task_locker.h"

struct xfs_m_inode_desc_t;

struct xfs_hd_super_block_desc_t
{
    uint32_t flags;
    uint32_t block_num;
    uint32_t block_size;
    int32_t inode_head;
    int32_t free_bitmap;
};

struct xfs_hd_inode_clause_desc_t
{
    uint32_t inode_count;
    int32_t start_offset;
    uint32_t size;
};

struct xfs_hd_inode_desc_t
{
    uint32_t flags;
    int16_t sub_driver_offset;
    int16_t main_driver_offset;
    int32_t pos_offset;
    int32_t owner_id;
    int32_t group_id;
    uint32_t size;
    struct xfs_hd_inode_clause_desc_t[0];
};

struct xfs_m_super_block_desc_t
{
    uint32_t flags;
    uint32_t block_num;
    uint32_t block_size;
    uint32_t inode_head;
    uint32_t free_bitmap;
    struct xfs_m_inode_desc_t* root_inode;
    struct block_buffer_weak_pointer_batch_desc_t bitmaps;  //Must in struct bottom
};

struct xfs_m_inode_desc_t
{
    struct vfs_inode_desc_t inode_base;
    struct block_buffer_weak_pointer_desc_t main_inode;
    struct block_buffer_weak_pointer_desc_t last_used_inode;
    uint32_t size;
};

static inline struct block_buffer_desc_t* xfs_load_super_block(uint16_t main_driver, uint16_t sub_driver, int timeout)
{
    long cur_jiffies = jiffies;
    struct block_buffer_desc_t* super_block_buffer = get_block_buffer(main_driver, sub_driver, 0, timeout);
    long used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(super_block_buffer == NULL)
        return NULL;
    cur_jiffies = jiffies;
    if(block_buffer_wait_op_finished(super_block_buffer, timeout) < 0)
        return NULL;
    kassert(!(super_block_buffer->flags & BLKBUFFER_FLAG_LOCK));
    used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(!(super_block_buffer->flags & BLKBUFFER_FLAG_UNSYNC))
        return super_block_buffer;
    if(sync_block_buffer(super_block_buffer, timeout) < 0)
        return NULL;
    return super_block_buffer;
}

void init_xfs_module()
{
}

int formatting_use_xfs(uint16_t main_driver, uint16_t sub_driver, int timeout)
{
    lock_task();
    struct block_buffer_desc_t* super_block_buffer = xfs_load_super_block(main_driver, sub_driver, timeout);
    if(super_block_buffer == NULL)
    {
        unlock_task();
        return NULL;
    }
    kassert(!(super_block_buffer->flags & BLKBUFFER_FLAG_UNSYNC));
    struct xfs_hd_super_block_desc_t* hd_super_block = (struct xfs_hd_super_block_desc_t*) super_block_buffer->buffer;
    unlock_task();
}