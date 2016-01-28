#include "block_buffer.h"

#include "string.h"
#include "mem.h"
#include "stddef.h"
#include "panic.h"
#include "block_driver.h"
#include "sched.h"
#include "blkbuffer_rb_tree.h"
#include "task_locker.h"
#include "printk.h"

static struct block_buffer_manager_info_t blk_buffer_manager;
static struct block_buffer_desc_t blank_buffer;

static size_t get_batch_block_buffer(struct block_buffer_desc_t* start, uint32_t flags, uint32_t tflags, struct block_buffer_desc_t* block_buffers[] , size_t max_len)
{
    lock_task();
    if(!(start->flags & flags) || start->flags & BLKBUFFER_FLAG_LOCK)
    {
        unlock_task();
        return 0;
    }
    kassert(start->flags & flags);
    kassert(!(start->flags & BLKBUFFER_FLAG_LOCK));
    kassert(max_len > 0);
    size_t i;
    size_t rml_count = 0;
    for(i = 0; i < max_len; ++i)
    {
        block_buffers[i] = start;
        if(start->used_count == 0)
        {
            kassert(
                circular_list_is_inlist(&blk_buffer_manager.free_list_head, &start->list_node) ||
                circular_list_is_inlist(&blk_buffer_manager.sync_list_head, &start->list_node) ||
                circular_list_is_inlist(&blk_buffer_manager.dirty_list_head, &start->list_node)
            );
            circular_list_remove(&start->list_node);
            ++rml_count;
        }

        kassert(!circular_list_is_inlist(&blk_buffer_manager.free_list_head, &start->list_node));
        kassert(!circular_list_is_inlist(&blk_buffer_manager.sync_list_head, &start->list_node));
        kassert(!circular_list_is_inlist(&blk_buffer_manager.dirty_list_head, &start->list_node));

        start->flags = tflags | BLKBUFFER_FLAG_LOCK;
        ++start->used_count;

        struct block_buffer_desc_t* next = parentof(
            rb_tree_next(&blk_buffer_manager.rb_tree_head, &start->rb_tree_node),
            struct block_buffer_desc_t,
            rb_tree_node
        );
        if(
            next == NULL ||
            start->driver_type != next->driver_type ||
            start->block_no + 1 != next->block_no ||
            !(next->flags & flags) ||
            next->flags & BLKBUFFER_FLAG_LOCK
        )
        {
            ++i;
            break;
        }
        start = next;
    }
    if(rml_count > 0)
    {
        int _res = ksemaphore_down(&blk_buffer_manager.inlist_sem, rml_count, -1);
        kassert(_res >= 0);
    }
    unlock_task();
    return i;
}

int block_buffer_wait_op_finished(struct block_buffer_desc_t* start, int timeout)
{
    if(start->flags & BLKBUFFER_FLAG_LOCK)
    {
        if(timeout < 0)
            return -1;
        if(timeout == 0)
        {
            while(start->flags & BLKBUFFER_FLAG_LOCK)
            {
                int res = kcond_wait(&start->op_cond, (uint32_t) timeout);
                if(res < 0)
                    return -1;
            }
        }
        else
        {
            while(start->flags & BLKBUFFER_FLAG_LOCK)
            {
                if(timeout <= 0)
                    return -1;
                long cur_jiffies = jiffies;
                int res = kcond_wait(&start->op_cond, (uint32_t) timeout);
                if(res < 0)
                    return -1;
                timeout -= jiffies - cur_jiffies;
            }
        }
    }
    return 0;
}

ssize_t flush_block_buffer(struct block_buffer_desc_t* start, int timeout)
{
    size_t max_len = block_driver_get_max_write(start->main_driver);
    struct block_buffer_desc_t* block_buffers[max_len];
    size_t len = get_batch_block_buffer(start, BLKBUFFER_FLAG_DIRTY, BLKBUFFER_FLAG_FLUSHING, block_buffers, max_len);
    if(len == 0)
        return -1;
    ssize_t blk_write_ret = block_driver_write_blocks(block_buffers, len, timeout);

    size_t twc = blk_write_ret < 0 ? 0 : (size_t) blk_write_ret;
    size_t i = 0;
    size_t in_list_count = 0;

    for(; i < twc; ++i)
    {
        kassert(block_buffers[i]->flags & BLKBUFFER_FLAG_FLUSHING);
        kassert(block_buffers[i]->flags & BLKBUFFER_FLAG_LOCK);
        kassert(block_buffers[i]->used_count != 0);
        --block_buffers[i]->used_count;
        block_buffers[i]->flags = block_buffers[i]->flags & BLKBUFFER_FLAG_DIRTY ? BLKBUFFER_FLAG_DIRTY : 0;
        kcond_broadcast(&block_buffers[i]->op_cond);
        if(block_buffers[i]->used_count == 0)
        {
            if(block_buffers[i]->flags & BLKBUFFER_FLAG_DIRTY)
                circular_list_insert(&blk_buffer_manager.dirty_list_head, &block_buffers[i]->list_node);
            else
                circular_list_insert(&blk_buffer_manager.sync_list_head, &block_buffers[i]->list_node);
            ++in_list_count;
        }
    }

    for(; i < len; ++i)
    {
        kassert(block_buffers[i]->flags & BLKBUFFER_FLAG_FLUSHING);
        kassert(block_buffers[i]->flags & BLKBUFFER_FLAG_LOCK);
        kassert(block_buffers[i]->used_count != 0);
        --block_buffers[i]->used_count;
        block_buffers[i]->flags = BLKBUFFER_FLAG_DIRTY;
        if(block_buffers[i]->used_count == 0)
        {
            circular_list_insert(&blk_buffer_manager.dirty_list_head, &block_buffers[i]->list_node);
            ++in_list_count;
        }
    }
    ksemaphore_up(&blk_buffer_manager.inlist_sem, in_list_count);
    return blk_write_ret;
}

ssize_t sync_block_buffer(struct block_buffer_desc_t* start, int timeout)
{
    size_t max_len = block_driver_get_max_read(start->main_driver);
    struct block_buffer_desc_t* block_buffers[max_len];
    size_t len = get_batch_block_buffer(start, BLKBUFFER_FLAG_UNSYNC, BLKBUFFER_FLAG_SYNCING, block_buffers, max_len);
    if(len == 0)
        return -1;
    ssize_t blk_read_ret = block_driver_read_blocks(block_buffers, len, timeout);

    size_t trc = blk_read_ret < 0 ? 0 : blk_read_ret;
    size_t i = 0;
    size_t in_list_count = 0;

    for(; i < trc; ++i)
    {
        kassert(block_buffers[i]->flags & BLKBUFFER_FLAG_SYNCING);
        kassert(block_buffers[i]->flags & BLKBUFFER_FLAG_LOCK);
        kassert(block_buffers[i]->used_count != 0);
        --block_buffers[i]->used_count;
        block_buffers[i]->flags = block_buffers[i]->flags & BLKBUFFER_FLAG_UNSYNC ? BLKBUFFER_FLAG_UNSYNC : 0;
        kcond_broadcast(&block_buffers[i]->op_cond);
        if(block_buffers[i]->used_count == 0)
        {
            if(block_buffers[i]->flags & BLKBUFFER_FLAG_UNSYNC)
            {
                rb_tree_remove(&blk_buffer_manager.rb_tree_head, &block_buffers[i]->rb_tree_node);
                circular_list_insert(&blk_buffer_manager.free_list_head, &block_buffers[i]->list_node);
            }
            else
                circular_list_insert(&blk_buffer_manager.sync_list_head, &block_buffers[i]->list_node);
            ++in_list_count;
        }
    }

    for(; i < len; ++i)
    {
        kassert(block_buffers[i]->flags & BLKBUFFER_FLAG_SYNCING);
        kassert(block_buffers[i]->flags & BLKBUFFER_FLAG_LOCK);
        kassert(block_buffers[i]->used_count != 0);
        --block_buffers[i]->used_count;
        block_buffers[i]->flags = BLKBUFFER_FLAG_UNSYNC;
        if(block_buffers[i]->used_count == 0)
        {
            rb_tree_remove(&blk_buffer_manager.rb_tree_head, &block_buffers[i]->rb_tree_node);
            circular_list_insert(&blk_buffer_manager.free_list_head, &block_buffers[i]->list_node);
            ++in_list_count;
        }
    }
    ksemaphore_up(&blk_buffer_manager.inlist_sem, in_list_count);
    return blk_read_ret;
}

void init_block_buffer_module()
{
    _memset(&blank_buffer, 0, sizeof(blank_buffer));
    rb_tree_init(&blk_buffer_manager.rb_tree_head, &blank_buffer.rb_tree_node);
    ksemaphore_init(&blk_buffer_manager.inlist_sem, BLKBUFFER_BLKCOUNT);
    circular_list_init(&blk_buffer_manager.free_list_head);
    circular_list_init(&blk_buffer_manager.sync_list_head);
    circular_list_init(&blk_buffer_manager.dirty_list_head);
    char* buffer_head = (char*) kgetpersistedpage(BLKBUFFER_BLKCOUNT / (4096 / BLKBUFFER_BLKSIZE));
    for(size_t i = 0; i < BLKBUFFER_BLKCOUNT; ++i)
    {
        struct block_buffer_desc_t* blk_desc = (struct block_buffer_desc_t*) kgetpersistedmem(sizeof(struct block_buffer_desc_t));
        _memset(blk_desc, 0, sizeof(struct block_buffer_desc_t));
        blk_desc->buffer = buffer_head;
        buffer_head += BLKBUFFER_BLKSIZE;
        kcond_init(&blk_desc->op_cond);
        blk_desc->flags = BLKBUFFER_FLAG_UNSYNC;
        circular_list_insert(&blk_buffer_manager.free_list_head, &(blk_desc->list_node));
    }
}

struct block_buffer_desc_t* get_block_buffer(uint16_t main_driver, uint16_t sub_driver, size_t block_no, int timeout)
{
    lock_task();
    struct block_buffer_desc_t* blk = parentof(
        blkbuffer_rb_tree_find(&blk_buffer_manager.rb_tree_head, main_driver, sub_driver, block_no),
        struct block_buffer_desc_t,
        rb_tree_node
    );
    if(blk == NULL)
    {
        if(ksemaphore_down(&blk_buffer_manager.inlist_sem, 1, timeout) != 1)
        {
            unlock_task();
            return NULL;
        }
        if(circular_list_is_empty(&blk_buffer_manager.free_list_head))
        {
            if(circular_list_is_empty(&blk_buffer_manager.sync_list_head))
            {
                kassert(!circular_list_is_empty(&blk_buffer_manager.dirty_list_head));
                long cur_jiffies = jiffies;
                if(flush_block_buffer(parentof(blk_buffer_manager.dirty_list_head.next, struct block_buffer_desc_t, list_node), timeout) < 0)
                {
                    unlock_task();
                    return NULL;
                }
                if(circular_list_is_empty(&blk_buffer_manager.sync_list_head))
                {
                    unlock_task();
                    return NULL;
                }
                if(timeout > 0)
                    timeout = timeout > (jiffies - cur_jiffies) ? timeout - (jiffies - cur_jiffies) : -1;
            }
            kassert(!circular_list_is_empty(&blk_buffer_manager.sync_list_head));
            struct block_buffer_desc_t* tmp = parentof(blk_buffer_manager.sync_list_head.next, struct block_buffer_desc_t, list_node);
            rb_tree_remove(&blk_buffer_manager.rb_tree_head, &tmp->rb_tree_node);
            tmp->flags = BLKBUFFER_FLAG_UNSYNC;
            circular_list_remove(&tmp->list_node);
            circular_list_insert(&blk_buffer_manager.free_list_head, &tmp->list_node);
        }
        blk = parentof(
            blkbuffer_rb_tree_find(&blk_buffer_manager.rb_tree_head, main_driver, sub_driver, block_no),
            struct block_buffer_desc_t,
            rb_tree_node
        );
        if(blk == NULL)
        {
            blk = parentof(blk_buffer_manager.free_list_head.next, struct block_buffer_desc_t, list_node);
            blk->flags = BLKBUFFER_FLAG_UNSYNC;
            blk->main_driver = main_driver;
            blk->sub_driver = sub_driver;
            blk->block_no = block_no;
            blk->used_count = 0;
            blkbuffer_rb_tree_insert(&blk_buffer_manager.rb_tree_head, &blk->rb_tree_node);
        }
    }
    if(blk->used_count == 0)
        circular_list_remove(&blk->list_node);
    blk->op_time = jiffies;
    ++blk->used_count;
    unlock_task();
    return blk;
}

void release_block_buffer(struct block_buffer_desc_t* blk)
{
    lock_task();
    --blk->used_count;
    if(blk->used_count == 0)
    {
        if(blk->flags & BLKBUFFER_FLAG_DIRTY)
            circular_list_insert(&blk_buffer_manager.dirty_list_head, &blk->list_node);
        else if(blk->flags & BLKBUFFER_FLAG_UNSYNC)
        {
            rb_tree_remove(&blk_buffer_manager.rb_tree_head, &blk->rb_tree_node);
            circular_list_insert(&blk_buffer_manager.free_list_head, &blk->list_node);
        }
        else
            circular_list_insert(&blk_buffer_manager.sync_list_head, &blk->list_node);
    }
    unlock_task();
}

struct block_buffer_desc_t* block_buffer_weak_pointer_get(struct block_buffer_weak_pointer_desc_t* weak_pointer, int timeout)
{
    lock_task();
    if(weak_pointer->block_buffer == NULL
        weak_pointer->driver_type != weak_pointer->block_buffer->driver_type ||
        weak_pointer->block_no != weak_pointer->block_buffer->driver_type ||
        weak_pointer->block_buffer->flags & BLKBUFFER_FLAG_UNSYNC
        )
        weak_pointer->block_buffer = get_block_buffer(weak_pointer->main_driver, weak_pointer->sub_driver, weak_pointer->block_no, timeout);
    else
    {
        if(weak_pointer->block_buffer->used_count == 0)
            circular_list_remove(&weak_pointer->block_buffer->list_node);
        weak_pointer->block_buffer->op_time = jiffies;
        ++weak_pointer->block_buffer->used_count;
    }
    unlock_task();
    return weak_pointer->block_buffer;
}

struct block_buffer_desc_t* block_buffer_weak_pointer_batch_get(struct block_buffer_weak_pointer_desc_t* weak_pointer, size_t num)
{
    lock_task();
    if(weak_pointer->block_buffers[num] == NULL
        weak_pointer->driver_type != weak_pointer->block_buffers[num]->driver_type ||
        weak_pointer->block_no + num != weak_pointer->block_buffers[num]->driver_type ||
        weak_pointer->block_buffers[num]->flags & BLKBUFFER_FLAG_UNSYNC
        )
        weak_pointer->block_buffers[num] = get_block_buffer(weak_pointer->main_driver, weak_pointer->sub_driver, weak_pointer->block_no + num, timeout);
    else
    {
        if(weak_pointer->block_buffers[num]->used_count == 0)
            circular_list_remove(&weak_pointer->block_buffers[num]->list_node);
        weak_pointer->block_buffers[num]->op_time = jiffies;
        ++weak_pointer->block_buffers[num]->used_count;
    }
    unlock_task();
    return weak_pointer->block_buffers[num];
}

void sys_test()
{
    struct block_buffer_desc_t* blk = get_block_buffer(3, 1, 100, 0);
    printk("blk_state: uc [%u] flags [%u] addr [%u]\n", blk->used_count, blk->flags, blk);
    if(blk->flags & BLKBUFFER_FLAG_UNSYNC)
    {
        int res = sync_block_buffer(blk, 0);
        printk("sync_res [%d]\n", res);
        kassert(res > 0);
        kassert(!(blk->flags & BLKBUFFER_FLAG_UNSYNC));
    }
    uint32_t* data = (uint32_t*) blk->buffer;
    printk("data[0] = %u\n", data[0]);
    data[0] = 1968;
    blk->flags |= BLKBUFFER_FLAG_DIRTY;
    release_block_buffer(blk);

    blk = get_block_buffer(3, 1, 100, 0);
    printk("blk_state: uc [%u] flags [%u] addr [%u]\n", blk->used_count, blk->flags, blk);
    data = (uint32_t*) blk->buffer;
    printk("data[0] = %u\n", data[0]);
    int res = flush_block_buffer(blk, 0);
    printk("flush_res [%d]\n", res);
    printk("blk_state: uc [%u] flags [%u]\n", blk->used_count, blk->flags);
    release_block_buffer(blk);
}