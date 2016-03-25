#pragma once

#include "stdint.h"

struct sys_params_t
{
    uint32_t memsize;
};

void init_sysparams();
struct sys_params_t* get_sysparams();
