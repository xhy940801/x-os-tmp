#pragma once

#include "stdint.h"

typedef struct
{
    uint32_t memsize;
} sys_params_t;

void init_sysparams();
sys_params_t* get_sysparams();
