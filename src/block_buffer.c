#include "block_buffer.h"

#include "string.h"
#include "mem.h"
#include "stddef.h"
#include "panic.h"
#include "block_driver.h"
#include "sched.h"
#include "blkbuffer_rb_tree.h"

static struct block_buffer_manager_info_t blk_buffer_manager;
static struct block_buffer_desc_t blank_buffer;

struct block_buffer_desc_t* flush_block_buffer(struct block_buffer_desc_t* start, int timeout)
{
    kassert(start->flags & BLKBUFFER_FLAG_DIRTY);
    kassert(!(start->flags & BLKBUFFER_FLAG_LOCK));
    struct block_buffer_desc_t* next = start;
    struct block_buffer_desc_t* end;
    size_t len = 0;
    size_t max_len = block_driver_get_max_write(start->main_driver);
    size_t rml_count = 0;
    do
    {
        end = next;
        if(end->flags & BLKBUFFER_FLAG_INQUEUE)
        {
            kassert(
                circular_list_is_inlist(&blk_buffer_manager.free_list_head, &end->list_node) ||
                circular_list_is_inlist(&blk_buffer_manager.sync_list_head, &end->list_node) ||
                circular_list_is_inlist(&blk_buffer_manager.dirty_list_head, &end->list_node)
            );

            circular_list_remove(&(end->list_node));
            end->flags &= ~BLKBUFFER_FLAG_INQUEUE;
            ++rml_count;
        }

        kassert(!circular_list_is_inlist(&blk_buffer_manager.free_list_head, &end->list_node));
        kassert(!circular_list_is_inlist(&blk_buffer_manager.sync_list_head, &end->list_node));
        kassert(!circular_list_is_inlist(&blk_buffer_manager.dirty_list_head, &end->list_node));

        end->flags = BLKBUFFER_FLAG_FLUSHING | BLKBUFFER_FLAG_LOCK;

        next = parentof(
            blkbuffer_rb_tree_next(&(blk_buffer_manager.rb_tree_head), &(end->rb_tree_node)),
            struct block_buffer_desc_t,
            rb_tree_node
        );
        ++len;
    } while(
        end->driver_type == next->driver_type &&
        end->block_no + 1 == next->block_no &&
        end->flags & BLKBUFFER_FLAG_DIRTY &&
        !(end->flags & BLKBUFFER_FLAG_LOCK) &&
        len < max_len
    );
    int _res = ksemaphore_down(&blk_buffer_manager.inlist_sem, rml_count, -1);
    kassert(_res >= 0);
    end = start;
    if(block_driver_write_blocks(start, len, timeout) < 0)
    {
        for(size_t i = 0; i < len; ++i)
        {
            kassert(end->flags & BLKBUFFER_FLAG_FLUSHING);
            kassert(end->flags & BLKBUFFER_FLAG_LOCK);
            kassert(!(end->flags & BLKBUFFER_FLAG_INQUEUE));
            circular_list_insert(&blk_buffer_manager.dirty_list_head, &end->list_node);
            end->flags = BLKBUFFER_FLAG_DIRTY | BLKBUFFER_FLAG_INQUEUE;
            next = parentof(
                blkbuffer_rb_tree_next(&(blk_buffer_manager.rb_tree_head), &(end->rb_tree_node)),
                struct block_buffer_desc_t,
                rb_tree_node
            );
            kassert(
                (i == len - 1) || (
                    end->driver_type == next->driver_type &&
                    end->block_no + 1 == next->block_no
                )
            );
            end = next;
        }
        return NULL;
    }
    else
    {
        for(size_t i = 0; i < len; ++i)
        {
            kassert(end->flags & BLKBUFFER_FLAG_FLUSHING);
            kassert(end->flags & BLKBUFFER_FLAG_LOCK);
            kassert(!(end->flags & BLKBUFFER_FLAG_INQUEUE));
            if(end->flags & BLKBUFFER_FLAG_DIRTY)
            {
                circular_list_insert(&blk_buffer_manager.dirty_list_head, &end->list_node);
                end->flags = BLKBUFFER_FLAG_DIRTY | BLKBUFFER_FLAG_INQUEUE;
            }
            else
            {
                circular_list_insert(&blk_buffer_manager.sync_list_head, &end->list_node);
                end->flags = BLKBUFFER_FLAG_INQUEUE;
            }
            next = parentof(
                blkbuffer_rb_tree_next(&(blk_buffer_manager.rb_tree_head), &(end->rb_tree_node)),
                struct block_buffer_desc_t,
                rb_tree_node
            );
            kassert(
                (i == len - 1) || (
                    end->driver_type == next->driver_type &&
                    end->block_no + 1 == next->block_no
                )
            );
            end = next;
        }
    }
    return end;
}

void block_buffer_module_init()
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
        blk_desc->flags |= BLKBUFFER_FLAG_INQUEUE;
        circular_list_insert(&blk_buffer_manager.free_list_head, &(blk_desc->list_node));
    }
}

struct block_buffer_desc_t* get_block_buffer(uint16_t main_driver, uint16_t sub_driver, size_t block_no, int timeout)
{
    struct block_buffer_desc_t* blk = parentof(
        blkbuffer_rb_tree_find(&blk_buffer_manager.rb_tree_head, main_driver, sub_driver, block_no),
        struct block_buffer_desc_t,
        rb_tree_node
    );
    if(blk == NULL)
    {
        if(ksemaphore_down(&blk_buffer_manager.inlist_sem, 1, timeout) != 1)
            return NULL;
        if(circular_list_is_empty(&blk_buffer_manager.free_list_head))
        {
            if(circular_list_is_empty(&blk_buffer_manager.sync_list_head))
            {
                kassert(!circular_list_is_empty(&blk_buffer_manager.dirty_list_head));
                long cur_jiffies = jiffies;
                if(flush_block_buffer(parentof(blk_buffer_manager.dirty_list_head.next, struct block_buffer_desc_t, list_node), timeout) == NULL)
                    return NULL;
                if(timeout > 0)
                    timeout = timeout > (jiffies - cur_jiffies) ? timeout - (jiffies - cur_jiffies) : -1;
            }
            kassert(!circular_list_is_empty(&blk_buffer_manager.sync_list_head));
            struct block_buffer_desc_t* tmp = parentof(blk_buffer_manager.sync_list_head.next, struct block_buffer_desc_t, list_node);
            rb_tree_remove(&blk_buffer_manager.rb_tree_head, &tmp->rb_tree_node);
            tmp->flags = BLKBUFFER_FLAG_UNSYNC | BLKBUFFER_FLAG_INQUEUE;
            circular_list_remove(&tmp->list_node);
            circular_list_insert(&blk_buffer_manager.free_list_head, &tmp->list_node);
        }
        blk = parentof(blk_buffer_manager.free_list_head.next, struct block_buffer_desc_t, list_node);
        circular_list_remove(&blk_buffer_manager.free_list_head);
        blk->flags = BLKBUFFER_FLAG_UNSYNC;
        blk->main_driver = main_driver;
        blk->sub_driver = sub_driver;
        blk->block_no = block_no;
        blkbuffer_rb_tree_insert(&blk_buffer_manager.rb_tree_head, &blk->rb_tree_node);
    }
    blk->op_time = jiffies;
    ++blk->used_count;
    return blk;
}

void release_block_buffer(struct block_buffer_desc_t* blk)
{
    --blk->used_count;
    if(blk->used_count == 0)
    {
        kassert(!(blk->flags & BLKBUFFER_FLAG_UNSYNC));
        if(blk->flags & BLKBUFFER_FLAG_DIRTY)
            circular_list_insert(&blk_buffer_manager.dirty_list_head, &blk->list_node);
        else
            circular_list_insert(&blk_buffer_manager.sync_list_head, &blk->list_node);
    }
}
