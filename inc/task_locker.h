#pragma once

#include "stddef.h"

struct task_locker_desc_t
{
    size_t lock_count;
};

void lock_task();

void unlock_task();

void v_lock_task();

void v_unlock_task();

size_t get_locker_count();

void set_locker_count(size_t count);
