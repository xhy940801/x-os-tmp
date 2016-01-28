#pragma once

#include "xfs.h"

#include "stdint.h"

#include "block_buffer.h"
#include "vfs.h"
#include "sched.h"
#include "task_locker.h"
#include "panic.h"

struct xfs_m_inode_desc_t;

struct xfs_hd_super_block_desc_t
{
    uint32_t flags;
    uint32_t block_num;
    uint32_t block_size;
    int32_t inode_head;
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

static inline uint32_t xfs_build_inode_flags(unsigned oauth, unsigned gauth, unsigned uauth, unsigned bigfile, unsigned inode_level)
{
    uint32_t flags = (oauth << 1);
    flags |= (gauth << 5);
    flags |= (uauth << 9);
    flags |= (bigfile << 16);
    flags |= (inode_level << 24);
    return flags;
}

static inline int xfs_formatting_init_root_inode(struct block_buffer_desc_t* root_inode_buffer)
{
    struct xfs_hd_inode_desc_t* inode = (struct xfs_hd_inode_desc_t*) root_inode_buffer->buffer;
    inode->flags = xfs_build_inode_flags(0, 7, 7, 0, 0);
    inode->owner_id = 0;
    inode->group_id = 0;
    inode->size = 0;
    return 0;
}

void init_xfs_module()
{
}

int formatting_use_xfs(uint16_t main_driver, uint16_t sub_driver, int timeout)
{
    struct block_driver_info_t driver_info;
    if(block_driver_get_driver_info(main_driver, sub_driver, &driver_info) < 0)
        return -1;
    if(driver_info->driver_block_count == 0 || driver_info->driver_block_size == 0)
        return -1;

    lock_task();
    long cur_jiffies = jiffies;
    struct block_buffer_desc_t* super_block_buffer = xfs_load_super_block(main_driver, sub_driver, timeout);
    long used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(super_block_buffer == NULL)
    {
        unlock_task();
        return -1;
    }
    kassert(!(super_block_buffer->flags & BLKBUFFER_FLAG_UNSYNC));
    struct xfs_hd_super_block_desc_t* hd_super_block = (struct xfs_hd_super_block_desc_t*) super_block_buffer->buffer;

    hd_super_block->flags = XFS_MAGIC_NO;
    hd_super_block->block_num = driver_info->driver_block_count / (4096 / driver_info->driver_block_size);
    hd_super_block->block_size = 4096;

    size_t bitmap_block_count = (hd_super_block->block_num + 4096 * 8 - 1) / (4096 * 8);
    hd_super_block->root_inode = bitmap_block_count + 1;

    size_t block_num = hd_super_block->block_num;
    size_t root_inode_pos = hd_super_block->root_inode;

    super_block_buffer->flags |= BLKBUFFER_FLAG_DIRTY;
    release_block_buffer(super_block_buffer);

    for(size_t i = 0; i < block_num / (4096 * 8); ++i)
    {
        cur_jiffies = jiffies;
        struct block_buffer_desc_t* bitmap = get_block_buffer(main_driver, sub_driver, i + 1, timeout);
        used_jiffies = jiffies - cur_jiffies;
        if(timeout > 0)
            timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
        if(bitmap == NULL)
        {
            unlock_task();
            return -1;
        }
        if(bitmap->flags & BLKBUFFER_FLAG_SYNCING)
        {
            cur_jiffies = jiffies;
            if(block_buffer_wait_op_finished(bitmap, timeout) < 0)
            {
                release_block_buffer(bitmap);
                unlock_task();
                return -1;
            }
            used_jiffies = jiffies - cur_jiffies;
            if(timeout > 0)
                timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
        }
        kassert(!(bitmap->flags & BLKBUFFER_FLAG_SYNCING));
        _memset(bitmap->buffer, 0, 4096);
        bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
        release_block_buffer(bitmap);
    }

    if(block_num % (4096 * 8) != 0)
    {
        cur_jiffies = jiffies;
        struct block_buffer_desc_t* bitmap = get_block_buffer(main_driver, sub_driver, block_num / (4096 * 8) + 1, timeout);
        used_jiffies = jiffies - cur_jiffies;
        if(timeout > 0)
            timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
        if(bitmap == NULL)
        {
            unlock_task();
            return -1;
        }
        if(bitmap->flags & BLKBUFFER_FLAG_SYNCING)
        {
            cur_jiffies = jiffies;
            if(block_buffer_wait_op_finished(bitmap, timeout) < 0)
            {
                release_block_buffer(bitmap);
                unlock_task();
                return -1;
            }
            used_jiffies = jiffies - cur_jiffies;
            if(timeout > 0)
                timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
        }
        kassert(!(bitmap->flags & BLKBUFFER_FLAG_SYNCING));
        _memset(bitmap->buffer, 0xffffffff, 4096);

        uint8_t* bitmap_st = (uint8_t*) bitmap->buffer;

        for(size_t i = 0; i < block_num % (4096 * 8); ++i)
            bitmap_st[i >> 3] &= (~(0x01 << (i & 0x07)));

        bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
        release_block_buffer(bitmap);
    }

    cur_jiffies = jiffies;
    struct block_buffer_desc_t* root_inode_buffer = get_block_buffer(main_driver, sub_driver, root_inode_pos, timeout);
    used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(root_inode_buffer == NULL)
    {
        unlock_task();
        return -1;
    }
    if(root_inode_buffer->flags & BLKBUFFER_FLAG_SYNCING)
    {
        cur_jiffies = jiffies;
        if(block_buffer_wait_op_finished(root_inode_buffer, timeout) < 0)
        {
            release_block_buffer(root_inode_buffer);
            unlock_task();
            return -1;
        }
        used_jiffies = jiffies - cur_jiffies;
        if(timeout > 0)
            timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    }
    kassert(!(root_inode_buffer->flags & BLKBUFFER_FLAG_SYNCING));

    int res = xfs_formatting_init_root_inode(root_inode_buffer);

    root_inode_buffer->flags |= BLKBUFFER_FLAG_DIRTY;
    release_block_buffer(root_inode_buffer);

    unlock_task();
    return res;
}