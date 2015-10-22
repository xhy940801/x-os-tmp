#include "panic.h"
#include "sysparams.h"
#include "printk.h"
#include "sched.h"
#include "mem.h"

int main1()
{
    init_sysparams();
    move_bios_mem();
    printk("\f");
    uint32_t memsize = get_sysparams()->memsize;
    printk("memsize: %u B = %u MB\n", memsize, memsize / (1024 * 1024));
    turn_to_process1();
    init_mem_desc();
    panic("could not run here!");
    return 0;
}
