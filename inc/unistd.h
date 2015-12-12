#pragma once

#include "stdint.h"

int fork();

void sched_yield();

void tsleep(uint32_t timeout);
