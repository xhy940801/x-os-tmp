#include "block_buffer.h"

#include "string.h"
#include "mem.h"
#include "stddef.h"

static struct block_buffer_manager_info_t blk_buffer_manager;
static struct block_buffer_desc_t blank_buffer;

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
        circular_list_insert(&blk_buffer_manager.free_list_head, &(blk_desc->list_node));
    }
}
