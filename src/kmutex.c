#include "kmutex.h"

#include "sched.h"
#include "wait.h"
#include "errno.h"
#include "panic.h"

void kmutex_init(struct kmutex_desc_t* mutex)
{
    circular_list_init(&(mutex->head));
}

int kmutex_lock(struct kmutex_desc_t* mutex, int timeout)
{
    lock_task();
    if(mutex->head.next == &(mutex->head))
    {
        circular_list_insert(&(mutex->head), &(cur_process->waitlist_node.node));
        unlock_task();
        return 0;
    }
    if(timeout < 0)
    {
        cur_process->last_errno = EAGAIN;
        unlock_task();
        return -1;
    }
    circular_list_insert(mutex->head.pre, &(cur_process->waitlist_node.node));
    out_sched_queue(cur_process);
    if(timeout > 0)
        ksleep(cur_process, (uint32_t) timeout);
    else
        kwait(cur_process);
    schedule();
    kassert(circular_list_is_inlist(&(mutex->head), &(cur_process->waitlist_node.node)) == 0);
    if(mutex->head.next ==  &(cur_process->waitlist_node.node))
    {
        circular_list_remove(&(cur_process->waitlist_node.node));
        unlock_task();
        return 0;
    }
    cur_process->last_errno = cur_process->sub_errno;
    unlock_task();
    return -1;
}

void kmutex_unlock(struct kmutex_desc_t* mutex)
{
    lock_task();
    kassert(circular_list_is_inlist(&(mutex->head), &(cur_process->waitlist_node.node)) == 0);
    kassert(mutex->head.next == &(cur_process->waitlist_node.node));
    circular_list_remove(&(cur_process->waitlist_node.node));
    if(mutex->head.next != &(mutex->head))
    {
        struct process_info_t* proc = parentof(mutex->head.next, struct process_info_t, waitlist_node.node);
        kwakeup(proc);
        in_sched_queue(proc);
    }
    unlock_task();
}
