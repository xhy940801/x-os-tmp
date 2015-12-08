#include "panic.h"

#include "printk.h"
#include "sched.h"

void panic(const char* str)
{
    printk("\x1b\x0c%s", str);
    _print_to_eax(cur_process->last_errno);
    while(1);
}

void _static_assert_func(const char* name, const char* file, unsigned int line, const char* func)
{
    printk("\x1b\x0c""assertion \"%s\" failed: file \"%s\""
            ", line %u, function: %s", name, file, line, func);
    while(1);
}
