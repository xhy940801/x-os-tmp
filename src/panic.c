#include "panic.h"

#include "printk.h"
#include "sched.h"
#include "task_locker.h"
#include "asm.h"

void panic(const char* str)
{
    lock_task();
    _cli();
    printk("\x1b\x0c%s", str);
    _print_to_eax(cur_process->last_errno);
    while(1);
    unlock_task();
}

void _static_assert_func(const char* name, const char* file, unsigned int line, const char* func)
{
    lock_task();
    printk(P_LRED "assertion \"%s\" failed: file \"%s\""
            ", line %u, function: %s", name, file, line, func);
    while(1);
    unlock_task();
}
