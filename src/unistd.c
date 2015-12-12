#include "unistd.h"

int fork()
{
    int _v;
    __asm__ (
        "mov $1, %%eax\n"
        "int $0x80\n"
        "mov %%eax, %0"
        :"=g"(_v)
        :
        :"eax","ebx","ecx","edx","memory"
    );
    return _v;
}

void sched_yield()
{
    __asm__ (
        "mov $2, %%eax\n"
        "int $0x80\n"
        :::"eax","ebx","ecx","edx","memory"
    );

}

void tsleep(uint32_t n)
{
    __asm__ (
        "mov $3, %%eax\n"
        "mov %0, %%ecx\n"
        "int $0x80\n"
        ::"g"(n)
        :"eax","ebx","ecx","edx","memory"
    );
}

