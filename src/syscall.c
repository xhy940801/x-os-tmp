#include "syscall.h"

#include "string.h"
#include "interrupt.h"
#include "printk.h"
#include "sched.h"
#include "panic.h"
#include "wait.h"
#include "vfs.h"
void sys_test();
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
    syscall_register(0x01, sys_fork);
    syscall_register(0x02, sys_yield);
    syscall_register(0x03, sys_tsleep);

    syscall_register(0x10, sys_write);
    syscall_register(0x11, sys_read);
    syscall_register(0x50, sys_test);
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
    int ret = syscalls[num].callback(arg0, arg1, arg2);
    // if(num == 1)
    //     printk("pic [%d] call [%d] errno [%d] ret [%d]\n", cur_process->pid, num, cur_process->last_errno, ret);
    return ret;
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
