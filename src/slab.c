#include "slab.h"

#include "string.h"
#include "mem.h"
#include "printk.h"
#include "panic.h"

static struct slab_slot_desc_t slab_slot_descs[8];
static struct slab_page_desc_t _slab_page_descs[16];

static inline void* page_start_pos(void* p)
{
    return (void*) (((uint32_t) p) & 0xfffff000);
}

void init_slab_module()
{
    _memset(slab_slot_descs, 0, sizeof(slab_slot_descs));
    for(size_t i = 0; i < 8; ++i)
    {
        size_t cur_alloc_size = (i + 1) * 16;
        slab_slot_descs[i].slab_capacity = (4096 - sizeof(struct slab_page_desc_t)) / cur_alloc_size;

        _slab_page_descs[i].alloc_count = slab_slot_descs[i].slab_capacity;
        _slab_page_descs[i].next = _slab_page_descs + i + 8;
        _slab_page_descs[i].pre = NULL;

        _slab_page_descs[i + 8].alloc_count = slab_slot_descs[i].slab_capacity;
        _slab_page_descs[i + 8].next = NULL;
        _slab_page_descs[i + 8].pre = _slab_page_descs + i;

        slab_slot_descs[i].free_page_head = _slab_page_descs + i;
        slab_slot_descs[i].free_slab_head = NULL;
    }
}

void* tslab_malloc(size_t size)
{
    kassert(size % 16 == 0 && size > 0 && size <= 128);
    size_t n = size / 16 - 1;
    if(slab_slot_descs[n].free_slab_head == NULL)
    {
        kassert(slab_slot_descs[n].free_slab_count == 0);

        struct slab_page_desc_t* pdesc = (struct slab_page_desc_t*) get_pages(0, 1, MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L);
        pdesc->alloc_count = 0;
        char* slab_head = (char*) pdesc->slab;
        struct slab_list_node_t* snode;
        for(size_t i = 0; i < slab_slot_descs[n].slab_capacity; ++i)
        {
            snode = (struct slab_list_node_t*) slab_head;
            slab_head += size;
            snode->next = (struct slab_list_node_t*) slab_head;
        }
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"
        snode->next = NULL;
#pragma GCC diagnostic pop
        kassert(page_start_pos(slab_head) == ((void*) pdesc));
        kassert(page_start_pos(slab_head + size) == ((void*) (((char*) pdesc) + 4096)));
        slab_slot_descs[n].free_slab_head = (struct slab_list_node_t*) pdesc->slab;

        pdesc->next = slab_slot_descs[n].free_page_head->next;
        pdesc->pre = slab_slot_descs[n].free_page_head;

        slab_slot_descs[n].free_page_head->next = pdesc;
        pdesc->next->pre = pdesc;

        ++slab_slot_descs[n].page_count;
        slab_slot_descs[n].free_slab_count += slab_slot_descs[n].slab_capacity;

        kassert(slab_slot_descs[n].free_slab_count == slab_slot_descs[n].slab_capacity);
    }

    struct slab_list_node_t* node = slab_slot_descs[n].free_slab_head;
    slab_slot_descs[n].free_slab_head = node->next;

    kassert(slab_slot_descs[n].free_slab_count > 0);
    
    --slab_slot_descs[n].free_slab_count;
    struct slab_page_desc_t* pdesc = (struct slab_page_desc_t*) page_start_pos(node);
    ++pdesc->alloc_count;
    return (void*) node;
}

void tslab_free(void* mem, size_t size)
{
    kassert(size % 16 == 0 && size > 0 && size <= 128);
    size_t n = size / 16 - 1;

    struct slab_list_node_t* node = (struct slab_list_node_t*) mem;
    node->next = slab_slot_descs[n].free_slab_head;
    slab_slot_descs[n].free_slab_head = node;

    struct slab_page_desc_t* pdesc = (struct slab_page_desc_t*) page_start_pos(mem);
    
    kassert(pdesc->alloc_count != 0);
    --pdesc->alloc_count;
    if(pdesc->alloc_count == 0)
    {
        pdesc->pre->next = pdesc->next;
        pdesc->next->pre = pdesc->pre;
        free_pages(pdesc, 0);
        
        kassert(slab_slot_descs[n].page_count > 0);

        --slab_slot_descs[n].page_count;
    }
}

void print_slab_status(size_t size)
{
    kassert(size % 16 == 0 && size > 0 && size <= 128);
    size_t n = size / 16 - 1;
    printk("page_count [%u] free_slab_count [%u] slab_capacity [%u]\n",
        slab_slot_descs[n].page_count,
        slab_slot_descs[n].free_slab_count,
        slab_slot_descs[n].slab_capacity
    );
}
