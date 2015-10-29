#include "mem.h"

#include "panic.h"
#include "printk.h"
#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include "limits.h"
#include "sysparams.h"
#include "asm.h"
#include "sched.h"
#include "buddy.h"

extern char _end[];
static struct mem_desc_t* mem_descs = NULL;
static struct mem_desc_t* free_mem_desc_head;
static struct ksemaphore_desc_t mem_semaphore;

#define KMEM_START 0xc0000000

static struct
{
    uint32_t end;
    uint32_t capacity;
} persisted_mem_desc;

static struct
{
    uint32_t cur_mem_pos;
} persisted_pagemem_desc;

static void init_mem_desc(uint32_t pagecount)
{
    _memset(mem_descs, 0, sizeof(struct mem_desc_t) * pagecount);
    kassert(persisted_mem_desc.capacity % 4096 == 0);
    uint32_t hasUsed = (persisted_mem_desc.capacity - KMEM_START) / 0x1000;
    printk("hasUsed [%u]\n", hasUsed);

    //less 640k
    for(size_t i = 0; i < 160; ++i)
        mem_descs[i].point = mem_descs + i + 1;
    //640k-1m
    for(size_t i = 160; i < 160 + 96; ++i)
    {
        mem_descs[i].active = USHRT_MAX;
        mem_descs[i].share = 1;
        mem_descs[i].flags = MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L;
        mem_descs[i].unused = 0;
        mem_descs[i].point = NULL;
    }
    //1m-2m
    for(size_t i = 256; i < 256 + 256; ++i)
    {
        mem_descs[i].active = USHRT_MAX;
        mem_descs[i].share = 1;
        mem_descs[i].flags = MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L | MEM_FLAGS_T;
        mem_descs[i].unused = 0;
        mem_descs[i].point = NULL;
    }
    //2m-end
    for(size_t i = 512; i < 256 + hasUsed; ++i)
    {
        mem_descs[i].active = USHRT_MAX;
        mem_descs[i].share = 1;
        mem_descs[i].flags = MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L;
        mem_descs[i].unused = 0;
        mem_descs[i].point = NULL;
    }
    //last-free
    for(size_t i = 256 + hasUsed; i < pagecount; ++i)
        mem_descs[i].point = mem_descs + i + 1;
    mem_descs[159].point = mem_descs + 256 + hasUsed;
    mem_descs[pagecount - 1].point = NULL;
    mem_descs[512].flags |= MEM_FLAGS_T;

    free_mem_desc_head = mem_descs;
    ksemaphore_init(&mem_semaphore, 160 + pagecount - 256 - hasUsed);
}

static void init_alloc_descs()
{
    persisted_pagemem_desc.cur_mem_pos = 0xffea5000;
}

static inline void mapping_page(uint32_t dst, uint32_t src, uint8_t type)
{
    kassert(dst % 4096 == 0 && src % 4096 == 0 && type < 8);
    uint32_t* page_table = (uint32_t*) (KMEM_START);
    page_table[(dst - KMEM_START) >> 12] = src | type;
}

static inline void unmapping_page(uint32_t dst)
{
    kassert(dst % 4096 == 0);
    uint32_t* page_table = (uint32_t*) (KMEM_START);
    page_table[(dst - KMEM_START) >> 12] = 0;
}

static inline uint32_t get_physical_addr(uint32_t dst)
{
    uint32_t* page_table = (uint32_t*) (KMEM_START);
    return page_table[(dst - KMEM_START) >> 12] & 0xfffff000;
}

//return 0 if not free page
static struct mem_desc_t* getonefreepage_unblocking(uint16_t share, uint16_t flags)
{
    if(free_mem_desc_head == NULL)
        return NULL;
#ifdef NDEBUG
    ksemaphore_down(&mem_semaphore, 1, 0);
#else
    int rs = ksemaphore_down(&mem_semaphore, 1, -1);
    kassert(rs == 1);
#endif
    struct mem_desc_t* cur = free_mem_desc_head;
    cur->active = USHRT_MAX;
    cur->share = share;
    cur->flags = flags;
    cur->unused = 0;
    free_mem_desc_head = (struct mem_desc_t*) cur->point;
    cur->point = NULL;
    return cur;
}

static struct mem_desc_t* getonefreepage(uint16_t share, uint16_t flags)
{
    ksemaphore_down(&mem_semaphore, 1, 0);
    kassert(free_mem_desc_head != NULL);
    struct mem_desc_t* cur = free_mem_desc_head;
    cur->active = USHRT_MAX;
    cur->share = share;
    cur->flags = flags;
    cur->unused = 0;
    free_mem_desc_head = (struct mem_desc_t*) cur->point;
    cur->point = NULL;
    return cur;
}

static void freeonepage(struct mem_desc_t* desc)
{
    kassert(desc->flags & MEM_FLAGS_P);
    desc->flags = 0;
    desc->point = (void*) free_mem_desc_head;
    free_mem_desc_head = desc;
    ksemaphore_up(&mem_semaphore, 1);
}

static void clear_page_table()
{
    uint32_t* page_table = (uint32_t*) (KMEM_START);
    uint32_t start_pos = (persisted_mem_desc.capacity - KMEM_START) >> 12;
    for(size_t i = start_pos; i < 4096; ++i)
        page_table[i] = 0;
    _lcr3(cur_process->cpu_state.catalog_table_p);
}

void init_mem_module()
{
    persisted_mem_desc.end = (uint32_t) _end;
    printk("kernel end at %u\n", persisted_mem_desc.end);
    uint32_t pagecount = get_sysparams()->memsize / (4096);
    mem_descs = (struct mem_desc_t*) persisted_mem_desc.end;

    persisted_mem_desc.end += sizeof(struct mem_desc_t) * pagecount;
    persisted_mem_desc.capacity = (persisted_mem_desc.end + 4095) & 0xfffff000;
    kassert(persisted_mem_desc.end < KMEM_START + 0x01000000);
    clear_page_table();
    init_mem_desc(pagecount);
    init_alloc_descs();
}


void* kgetpersistedmem(size_t size)
{
    kassert(!(size & 3));
    void* pos = (void*) persisted_mem_desc.end;
    do
    {
        if(persisted_mem_desc.end + size < persisted_mem_desc.capacity)
        {
            persisted_mem_desc.end += size;
            _memset(pos, 0, size);
            return pos;
        }
        kassert(mem_descs);
        struct mem_desc_t* desc = getonefreepage_unblocking(1, MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L);
        kassert(desc);
        mapping_page(persisted_mem_desc.capacity, (desc - mem_descs) * 4096, 3);
        persisted_mem_desc.capacity += 4096;
    } while(1);
}

void* kgetpersistedpage(size_t size)
{
    while(size--)
    {
        persisted_pagemem_desc.cur_mem_pos -= 4096;
        struct mem_desc_t* desc = getonefreepage_unblocking(1, MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L);
        kassert(desc);
        mapping_page(persisted_pagemem_desc.cur_mem_pos, (desc - mem_descs) * 4096, 3);
    }
    _memset((void*) persisted_pagemem_desc.cur_mem_pos, 0, size * 4096);
    return (void*) persisted_pagemem_desc.cur_mem_pos;
}

uint32_t get_free_mem_size()
{
    uint32_t fc = 0;
    struct mem_desc_t* p = free_mem_desc_head;
    while(p)
    {
        ++fc;
        p = (struct mem_desc_t*) p->point;
    }
    kassert(fc == mem_semaphore.surplus);
    return fc * 4096;
}

void* get_pages(size_t n, uint16_t share, uint16_t flags)
{
    void* p = get_address_area(n);
    size_t pcount = 1 << n;
    uint32_t cp = (uint32_t) p;
    for(size_t i = 0; i < pcount; ++i)
    {
        struct mem_desc_t* desc = getonefreepage(share, flags);
        mapping_page(cp, (desc - mem_descs) * 4096, 7);
        cp += 4096;
    }
    _memset(p, 0, pcount * 4096);
    return p;
}

void free_pages(void* p, size_t n)
{
    size_t pcount = 1 << n;
    uint32_t cp = (uint32_t) p;
    for(size_t i = 0; i < pcount; ++i)
    {
        struct mem_desc_t* desc = mem_descs + (get_physical_addr(cp) / 4096);
        unmapping_page(cp);
        freeonepage(desc);
        cp += 4096;
    }
    free_address_area(p, n);
    flush_page_table();
}

void flush_page_table()
{
    _lcr3(cur_process->cpu_state.catalog_table_p);
}
