#include "ksemaphore.h"

#include "sched.h"
#include "wait.h"
#include "errno.h"
#include "panic.h"

void ksemaphore_init(struct ksemaphore_desc_t* sem, size_t surplus)
{
    circular_list_init(&(sem->head));
    sem->surplus = surplus;
}

void ksemaphore_up(struct ksemaphore_desc_t* sem, size_t num)
{
    sem->surplus += num;
    while(1)
    {
        if(sem->head.next == &(sem->head))
            return;
        struct process_info_t* proc = parentof(sem->head.next, struct process_info_t, waitlist_node.node);
        if(proc->waitlist_node.demand > sem->surplus)
            return;
        kwakeup(proc);
        in_sched_queue(proc);
        circular_list_remove(&(proc->waitlist_node.node));
        sem->surplus -= proc->waitlist_node.demand;
        proc->waitlist_node.demand = 0;
    }
}

int ksemaphore_down(struct ksemaphore_desc_t* sem, size_t demand, int timeout)
{
    kassert(demand > 0);
    if(sem->surplus >= demand && sem->head.next == &(sem->head))
    {
        sem->surplus -= demand;
        return demand;
    }

    if(timeout < 0)
    {
        cur_process->last_errno = EAGAIN;
        return -1;
    }
    cur_process->waitlist_node.demand = demand;
    circular_list_insert(sem->head.pre, &(cur_process->waitlist_node.node));
    out_sched_queue(cur_process);
    if(timeout > 0)
        ksleep(cur_process, (uint32_t) timeout);
    else
        kwait(cur_process);
    schedule();
    if(cur_process->waitlist_node.demand == 0)
        return demand;
    cur_process->last_errno = cur_process->sub_errno;
    return -1;
}
