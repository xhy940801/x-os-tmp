#include "buddy.h"

#include "mem.h"
#include "stddef.h"
#include "stdint.h"
#include "panic.h"
#include "printk.h"

#define BUDDY_DEP 15

static struct buddy_alloc_t buddy_allocs[BUDDY_DEP];

void init_buddy_module()
{
    printk("buddy alloc pos: %u\n", buddy_allocs);
    uint32_t list_size = 262144;
    for(size_t i = 0; i < BUDDY_DEP; ++i, list_size >>= 1)
    {
        buddy_allocs[i].map = (uint32_t*) kgetpersistedmem((list_size / 16) < 4 ? 4 : (list_size / 16));
        buddy_allocs[i].list_start = (struct buddy_list_node_t*) kgetpersistedmem(list_size * sizeof(struct buddy_list_node_t));
        buddy_allocs[i].list_head = NULL;
    }
    uint32_t lb_size = 262144 / (1 << (BUDDY_DEP - 1));
    for(size_t i = 0; i < lb_size; ++i)
    {
        buddy_allocs[BUDDY_DEP - 1].list_start[i].next = &(buddy_allocs[BUDDY_DEP - 1].list_start[i + 1]);
        buddy_allocs[BUDDY_DEP - 1].list_start[i].prev = &(buddy_allocs[BUDDY_DEP - 1].list_start[i - 1]);
    }
    printk("lbsize %u\n", lb_size);
    buddy_allocs[BUDDY_DEP - 1].list_start[0].prev = NULL;
    buddy_allocs[BUDDY_DEP - 1].list_start[lb_size - 1].next = NULL;
    buddy_allocs[BUDDY_DEP - 1].list_head = buddy_allocs[BUDDY_DEP - 1].list_start;

    buddy_allocs[BUDDY_DEP - 1].map[0] |= 1;
    buddy_allocs[BUDDY_DEP - 1].map[(lb_size >> 1) / 32] |= (1 << ((lb_size >> 1) % 32 - 1));
    buddy_allocs[BUDDY_DEP - 1].list_head =  buddy_allocs[BUDDY_DEP - 1].list_start + 1;
    buddy_allocs[BUDDY_DEP - 1].list_start[1].prev = NULL;
    buddy_allocs[BUDDY_DEP - 1].list_start[lb_size - 2].next = NULL;
}

void* get_address_area(size_t n)
{
    if(n >= BUDDY_DEP)
        return NULL;
    if(buddy_allocs[n].list_head != NULL)
    {
        struct buddy_list_node_t* node = buddy_allocs[n].list_head;
        buddy_allocs[n].list_head = node->next;
        if(buddy_allocs[n].list_head != NULL)
            buddy_allocs[n].list_head->prev = NULL;
        uint32_t pos = node - buddy_allocs[n].list_start;
        //printk("in buddy [%u] pos [%u]\n", n, pos);
        buddy_allocs[n].map[(pos >> 1) / 32] ^= (1 << ((pos >> 1) % 32));
        return (void*) (pos * (1 << n) * 4096 + MEM_START);
    }
    void* parent = get_address_area(n + 1);
    if(parent == NULL)
        return NULL;
    uint32_t pos = (uint32_t) parent;
    pos = (pos - MEM_START) / 4096;
    pos /= (1 << n);
    //printk("parent pos [%u] at [%u]\n", pos, n);
    buddy_allocs[n].map[(pos >> 1) / 32] ^= (1 << ((pos >> 1) % 32));
    buddy_allocs[n].list_start[pos + 1].next = buddy_allocs[n].list_head;
    buddy_allocs[n].list_start[pos + 1].prev = NULL;
    if(buddy_allocs[n].list_head != NULL)
        buddy_allocs[n].list_head->prev = &(buddy_allocs[n].list_start[pos + 1]);
    buddy_allocs[n].list_head = &(buddy_allocs[n].list_start[pos + 1]);
    return parent;
}

void free_address_area(void* p, size_t n)
{
    kassert(n < BUDDY_DEP);
    kassert(((uint32_t) p) >= MEM_START);
    uint32_t pos = (uint32_t) p;
    pos = (pos - MEM_START) / 4096;
    pos /= (1 << n);
    buddy_allocs[n].map[(pos >> 1) / 32] ^= (1 << ((pos >> 1) % 32));
    if(buddy_allocs[n].map[(pos >> 1) / 32] & (1 << ((pos >> 1) % 32)) || n == BUDDY_DEP - 1)
    {
        buddy_allocs[n].list_start[pos].next = buddy_allocs[n].list_head;
        buddy_allocs[n].list_start[pos].prev = NULL;
        if(buddy_allocs[n].list_head != NULL)
            buddy_allocs[n].list_head->prev = &(buddy_allocs[n].list_start[pos]);
        buddy_allocs[n].list_head = &(buddy_allocs[n].list_start[pos]);
        return;
    }
    uint32_t buddy = pos ^ 1;
    if(buddy_allocs[n].list_start[buddy].prev != NULL)
        buddy_allocs[n].list_start[buddy].prev->next = buddy_allocs[n].list_start[buddy].next;
    else
        buddy_allocs[n].list_head = buddy_allocs[n].list_start[buddy].next;

    if(buddy_allocs[n].list_start[buddy].next != NULL)
        buddy_allocs[n].list_start[buddy].next->prev = buddy_allocs[n].list_start[buddy].prev;

    free_address_area(p, n + 1);
}
