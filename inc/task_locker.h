#pragma once

#include "stddef.h"

struct task_locker_desc_t
{
    size_t lock_count;
};

void lock_task();

void unlock_task();
