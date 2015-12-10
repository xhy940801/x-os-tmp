#include "panic.h"
#include "sysparams.h"
#include "printk.h"
#include "sched.h"
#include "mem.h"
#include "buddy.h"
#include "slab.h"
#include "vfs.h"
#include "tty0.h"
#include "syscall.h"

void print_mem_status()
{
    uint32_t free_memsize = get_free_mem_size();
    printk("%s: free memsize: %u B = %u MB\n", __func__, free_memsize, free_memsize / (1024 * 1024));
}

void test_pages()
{
    const uint32_t SIZE = 100;
    void* pages[SIZE];
    printk("\n");
    for(size_t i = 0; i < SIZE; ++i)
    {
        pages[i] = get_pages(0, 1, MEM_FLAGS_P | MEM_FLAGS_K | MEM_FLAGS_L);
        char* p = pages[i];
        p[1000] = 1;
        printk("new get\n");
    }
    for(size_t i = 0; i < SIZE; ++i)
        free_pages(pages[i], 0);
}

void test_slab()
{
    const uint32_t SIZE = 800;
    void* allocs[SIZE];
    for(size_t i = 0; i < SIZE; ++i)
    {
        printk("i %u", i);
        allocs[i] = tslab_malloc(128);
    }
    print_slab_status(128);
    print_mem_status();
    for(size_t i = 0; i < SIZE; ++i)
        tslab_free(allocs[SIZE - i - 1], 128);
    print_slab_status(128);
    uint32_t free_memsize = get_free_mem_size();
    printk("%s: free memsize: %u B = %u MB\n", __func__, free_memsize, free_memsize / (1024 * 1024));
}

void user_do()
{
    int ret;
    __asm__ (
        "mov $1, %%eax\n"
        "int $0x80\n"
        "mov %%eax, %0"
        :"=g"(ret)
        :
        :"eax","ebx","ecx","edx","memory"
    );
    if(ret == 0)
    {
        while(1)
            __asm__ volatile("nop;nop");
    }
    while(1);
}

int main1()
{
    init_sysparams();
    init_vfs_module();
    init_tty0_module();
    turn_to_process1();
    init_paging_module();

    printk("\f");
    uint32_t memsize = get_sysparams()->memsize;
    printk("memsize: %u B = %u MB\n", memsize, memsize / (1024 * 1024));
    init_mem_module();
    init_buddy_module();
    uint32_t free_memsize = get_free_mem_size();
    printk("free memsize: %u B = %u MB\n", free_memsize, free_memsize / (1024 * 1024));
    init_slab_module();
    //test_pages();
    //test_slab();
    //printk("free memsize: %u B = %u MB\n", free_memsize, free_memsize / (1024 * 1024));

    init_syscall_module();
    iret_to_user_level(user_do);
    panic("could not run here!");
    return 0;
}


