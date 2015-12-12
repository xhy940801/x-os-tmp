#pragma once

#include "stdint.h"
#include "stddef.h"

#define syscall0(num) ({\
    int _v;\
    __asm__ volatile(\
        "mov %1, %%eax\n"\
        "int $0x80\n"\
        "mov %%eax, %0"\
        :"=g"(_v)\
        :"g"(num)\
        :"eax","ebx","ecx","edx","memory"\
    );\
    _v;\
})

#define syscall1(num, arg0) ({\
    int _v;\
    __asm__ volatile(\
        "mov %1, %%eax\n"\
        "mov %2, %%ecx\n"\
        "int $0x80\n"\
        "mov %%eax, %0"\
        :"=g"(_v)\
        :"g"(num),"g"(arg0)\
        :"eax","ebx","ecx","edx","memory"\
    );\
    _v;\
})

#define syscall2(num, arg0, arg1) ({\
    int _v;\
    __asm__ volatile(\
        "mov %1, %%eax\n"\
        "mov %2, %%ecx\n"\
        "mov %3, %%edx\n"\
        "int $0x80\n"\
        "mov %%eax, %0"\
        :"=g"(_v)\
        :"g"(num),"g"(arg0),"g"(arg1)\
        :"eax","ebx","ecx","edx","memory"\
    );\
    _v;\
})

#define syscall3(num, arg0, arg1, arg2) ({\
    int _v;\
    __asm__ volatile(\
        "mov %1, %%eax\n"\
        "mov %2, %%ecx\n"\
        "mov %3, %%edx\n"\
        "mov %4, %%ebx\n"\
        "int $0x80\n"\
        "mov %%eax, %0"\
        :"=g"(_v)\
        :"g"(num),"g"(arg0),"g"(arg1),"g"(arg2)\
        :"eax","ebx","ecx","edx","memory"\
    );\
    _v;\
})


int fork();

void sched_yield();

void tsleep(uint32_t timeout);

void write(int fd, const char* buf, size_t len);

void read(int fd, char* buf, size_t len);
