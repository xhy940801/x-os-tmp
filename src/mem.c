#include "mem.h"

#include "panic.h"
#include "printk.h"
#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include "limits.h"
#include "sysparams.h"

extern char _end[];
static mem_desc_t* mem_descs = NULL;
static mem_desc_t* free_mem_desc_head;
static uint32_t free_page_count;

//return 0 if not free page
mem_desc_t* getonefreepage_unblocking(uint16_t share, uint16_t flags)
{
    if(free_mem_desc_head == NULL)
        return NULL;
    mem_desc_t* cur = free_mem_desc_head;
    cur->active = USHRT_MAX;
    cur->share = share;
    cur->flags = flags;
    cur->unused = 0;
    free_mem_desc_head = (mem_desc_t*) cur->point;
    cur->point = NULL;
    --free_page_count;
    return cur;
}

void mapping_page(uint32_t* page_catalog_table, uint32_t dst, uint32_t src, uint8_t type)
{
    kassert(page_catalog_table[dst >> 22] & 0x01);
    uint32_t* page_table = (uint32_t*) (page_catalog_table[dst >> 22] & 0xfffff000);
    page_table[(dst >> 12) & 0x3ff] = (src & 0xfffff000) | type;
}

void move_bios_mem()
{
    _memset((void*) 0x5000, 0, 0x1000);
    for(uint32_t dst = MEM_START + 1023 * 1024 * 1024 + 640 * 1024, src = 640 * 1024;
            src < 1024 * 1024; src += 4096, dst += 4096)
        mapping_page((uint32_t*) 0, dst, src, 3);

    uint32_t pagecount = get_sysparams()->memsize / (4096);
    for(uint32_t dst = 640 * 1024 + MEM_START, src = (pagecount - 96) * 4096;
            dst < 1024 * 1024 + MEM_START; dst += 4096, src += 4096)
        mapping_page((uint32_t*) 0, dst, src, 3);
}

void* kgetpersistedmem(size_t size)
{
    static uint32_t cur_mem_pos = ((uint32_t) _end);
    static uint32_t cur_mem_capacity = MEM_START + 16 * 1024 * 1024;
    kassert(!(size & 3));
    void* pos = (void*) cur_mem_pos;
    do
    {
        if(cur_mem_pos + size < cur_mem_capacity)
        {
            cur_mem_pos += size;
            _memset(pos, 0, size);
            return pos;
        }
        kassert(mem_descs);
        mem_desc_t* desc = getonefreepage_unblocking(1, MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L);
        kassert(desc);
        mapping_page((uint32_t*) 0, cur_mem_capacity, (desc - mem_descs) * 4096, 3);
        cur_mem_capacity += 4096;
    } while(1);
}

void* kgetpersistedpage(size_t size)
{
    static uint32_t cur_mem_pos = 0xffea5000;
    while(size--)
    {
        cur_mem_pos -= 4096;
        mem_desc_t* desc = getonefreepage_unblocking(1, MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L);
        kassert(desc);
        mapping_page((uint32_t*) 0, cur_mem_pos, (desc - mem_descs) * 4096, 3);
    }
    _memset((void*) cur_mem_pos, 0, size * 4096);
    return (void*) cur_mem_pos;
}

void init_mem_desc()
{
    uint32_t end = (uint32_t) _end;
    printk("kernel end at %u\n", end);
    uint32_t pagecount = get_sysparams()->memsize / (4096);
    kassert(pagecount * sizeof(mem_desc_t) < 16 * 1024 * 1024 - end);
    mem_descs = kgetpersistedmem(pagecount * sizeof(mem_desc_t));
    for(uint32_t i = 0; i < pagecount - 96 - 251; ++i)
    {
        _memset(mem_descs + i, 0, sizeof(mem_desc_t));
        mem_descs[i].point = mem_descs + i + 1;
    }
    mem_descs[pagecount - 96 - 251 - 1].point = NULL;
    for(uint32_t i = pagecount - 96 - 251, offset = 768 + 4; i < pagecount - 96; ++i, ++offset)
    {
        uint32_t* catalog = (uint32_t*) 0;
        catalog[offset] = (i << 12) | 0x03;
        mem_descs[i].active = USHRT_MAX;
        mem_descs[i].share = 1;
        mem_descs[i].flags = MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L | MEM_FLAGS_T;
        mem_descs[i].unused = 0;
        mem_descs[i].point = NULL;
    }
    for(uint32_t i = pagecount - 96; i < pagecount; ++i)
    {
        mem_descs[i].active = USHRT_MAX;
        mem_descs[i].share = 1;
        mem_descs[i].flags = MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L;
        mem_descs[i].unused = 0;
        mem_descs[i].point = NULL;
    }
    for(uint32_t i = 0; i < 4096; ++i)
    {
        mem_descs[i].active = USHRT_MAX;
        mem_descs[i].share = 1;
        mem_descs[i].flags = MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L;
        mem_descs[i].unused = 0;
        mem_descs[i].point = NULL;
    }
    for(uint32_t i = 0; i < 6; ++i)
        mem_descs[i].flags |= MEM_FLAGS_T;
    free_mem_desc_head = mem_descs + 4096;
    free_page_count = pagecount - 4096 - 96 - 251;
    //test
    uint32_t ckcount = 0;
    for(mem_desc_t* head = free_mem_desc_head; head != NULL; head = (mem_desc_t*) head->point)
        ++ckcount;
    kassert(ckcount == free_page_count);
    //test-end
}
