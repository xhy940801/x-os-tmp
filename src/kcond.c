#include "kcond.h"

#include "sched.h"
#include "task_locker.h"

void kcond_init(struct kcond_desc_t* cond)
{
    circular_list_init(&cond->head);
}

int kcond_wait(struct kcond_desc_t* cond, uint32_t timeout)
{
    lock_task();
    circular_list_insert(cond->head.pre, &cur_process->waitlist_node.node);
    out_sched_queue(cur_process);
    cur_process->waitlist_node.waiting = 1;
    if(timeout > 0)
        ksleep(cur_process, (uint32_t) timeout);
    else
        kwait(cur_process);
    schedule();
    if(cur_process->waitlist_node.waiting != 0)
    {
        cur_process->last_errno = cur_process->sub_errno;
        unlock_task();
        return -1;
    }
    unlock_task();
    return 0;
}

unsigned int kcond_signal(struct kcond_desc_t* cond)
{
    lock_task();
    if(circular_list_is_empty(&cond->head))
    {
        unlock_task();
        return 0;
    }
    struct process_info_t* proc = parentof(cond->head.next, struct process_info_t, waitlist_node.node);
    circular_list_remove(cond->head.next);
    kwakeup(proc);
    in_sched_queue(proc);
    proc->waitlist_node.waiting = 0;
    unlock_task();
    return 1;
}

unsigned int kcond_broadcast(struct kcond_desc_t* cond)
{
    int count = 0;
    lock_task();
    while(!circular_list_is_empty(&cond->head))
    {

        struct process_info_t* proc = parentof(cond->head.next, struct process_info_t, waitlist_node.node);
        circular_list_remove(cond->head.next);
        kwakeup(proc);
        in_sched_queue(proc);
        proc->waitlist_node.waiting = 0;
        ++count;
    }
    unlock_task();
    return count;
}
