#include "exec.h"

#include "string.h"
#include "sched.h"
#include "panic.h"
#include "printk.h"

extern char _end[];

int load_program(void* target, struct process_info_t* process_info)
{
	if((uint32_t) target > 0xa000000 && (uint32_t) target < 0xc000000)
		return 0;
    void* src = (void*) ((char*) target + 0xc0000000);
    printk("src [%u] target [%u]\n", src, target);
    kassert((uint32_t) src < (uint32_t) _end);
    _memcpy(target, src, 4096);
    return 0;
}
