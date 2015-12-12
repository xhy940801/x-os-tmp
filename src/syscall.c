#include "syscall.h"

#include "string.h"
#include "interrupt.h"
#include "printk.h"
#include "sched.h"
#include "panic.h"
#include "wait.h"

void sys_call();

struct syscall_desc_t
{
    int (*callback)(int arg0, int arg1, int arg2);
};

struct syscall_desc_t syscalls[SYSCALL_SIZE];

void init_syscall_module()
{
    setup_trap_desc(0x80, sys_call, 3);
    for(size_t i = 0; i < SYSCALL_SIZE; ++i)
        syscalls[i].callback = NULL;
    syscall_register(1, sys_fork);
    syscall_register(2, sys_yield);
    syscall_register(3, sys_tsleep);
}

int sys_call_fail(int n)
{
    printk("syscall fail: n [%d]", n);
    return -1;
}

int t_sys_call(int num, int arg0, int arg1, int arg2)
{
    if(num > SYSCALL_SIZE || syscalls[num].callback == NULL)
        return -1;
    return syscalls[num].callback(arg0, arg1, arg2);
}

int syscall_register(int num, void* callback)
{
    if(num > SYSCALL_SIZE)
        return -1;
    if(syscalls[num].callback != NULL)
        return -2;
    syscalls[num].callback = (int (*)()) callback;
    return 0;
}
