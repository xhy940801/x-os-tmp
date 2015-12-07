#include "block_buffer.h"

#include "string.h"
#include "mem.h"
#include "stddef.h"

static struct block_buffer_manager_info_t blk_buffer_manager;
static struct block_buffer_desc_t blank_buffer;

int flush_block_buffer(struct block_buffer_desc_t* start)
{
    kassert(start->flags & BLKBUFFER_FLAG_DIRTY);
    kassert(!(start->flags & BLKBUFFER_FLAG_LOCK));
    struct block_buffer_desc_t* end = start;
    size_t len = 0;
    size_t max_len = block_driver_get_max_write(start->main_driver);
    do
    {
        if(end->flags & BLKBUFFER_FLAG_INQUEUE)
        {
            kassert(
                circular_list_inlist(&blk_buffer_manager.free_list_head, &end->list_node) ||
                circular_list_inlist(&blk_buffer_manager.sync_list_head, &end->list_node) ||
                circular_list_inlist(&blk_buffer_manager.dirty_list_head, &end->list_node)
            );

            circular_list_remove(&(end->list_node));
            end->flags &= ^BLKBUFFER_FLAG_INQUEUE;
        }

        kassert(!circular_list_inlist(&blk_buffer_manager.free_list_head, &end->list_node));
        kassert(!circular_list_inlist(&blk_buffer_manager.sync_list_head, &end->list_node));
        kassert(!circular_list_inlist(&blk_buffer_manager.dirty_list_head, &end->list_node));

        end->flags = (BLKBUFFER_FLAG_FLUSHING | BLKBUFFER_FLAG_LOCK);

        struct block_buffer_desc_t* next = parentof(
            blkbuffer_rb_tree_next(&(blk_buffer_manager.rb_tree_head), &(end->rb_tree_node)),
            struct block_buffer_desc_t,
            rb_tree_node
            );
        ++len;
        end = next;
    } while(
        end->driver_type == next->driver_type &&
        end->block_no + 1 == next->block_no &&
        end->flags & BLKBUFFER_FLAG_DIRTY &&
        !(end->flags & BLKBUFFER_FLAG_LOCK) &&
        len < max_len
    );
    
}

void block_buffer_module_init()
{
    _memset(&blank_buffer, 0, sizeof(blank_buffer));
    rb_tree_init(&blk_buffer_manager.rb_tree_head, &blank_buffer.list_node);
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

struct block_buffer_desc_t* get_block_buffer(uint16_t main_driver, uint16_t sub_driver, size_t block_no)
{
    struct block_buffer_desc_t* blk = parentof(
        blkbuffer_rb_tree_find(&blk_buffer_manager.rb_tree_head, main_driver, sub_driver, block_no),
        struct block_buffer_manager_info_t,
        rb_tree_head
        );
    if(blk == NULL)
    {
        if(ksemaphore_down(&blk_buffer_manager.inlist_sem, 1, 0) != 1)
            return NULL;
        while(circular_list_is_empty() && )
    }
}
