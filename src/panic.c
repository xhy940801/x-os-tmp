#include "panic.h"

#include "printk.h"
#include "sched.h"

void panic(const char* str)
{
    printk("\033[32m%s", str);
    while(1);
}

void _static_assert_func(const char* name, const char* file, unsigned int line, const char* func)
{
    printk("\033[32;35;31m\x0c""assertion \"%s\" failed: file \"%s\""
            ", line %u, function: %s", name, file, line, func);
    while(1);
}
