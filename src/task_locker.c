#include "task_locker.h"

#include "sched.h"
#include "panic.h"
#include "asm.h"

void lock_task()
{
    if(cur_process->task_locker.lock_count == 0)
        _cli();
    ++cur_process->task_locker.lock_count;
}

void unlock_task()
{
    kassert(cur_process->task_locker.lock_count > 0);
    --cur_process->task_locker.lock_count;
    if(cur_process->task_locker.lock_count == 0)
        _sti();
}

void v_lock_task()
{
    ++cur_process->task_locker.lock_count;
}

void v_unlock_task()
{
    --cur_process->task_locker.lock_count;
}

size_t get_locker_count()
{
    return cur_process->task_locker.lock_count;
}

void set_locker_count(size_t count)
{
    cur_process->task_locker.lock_count = count;
}
