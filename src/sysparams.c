#include "stdint.h"
#include "sysparams.h"

static sys_params_t sys_params;

void init_sysparams()
{
    void* param_pos = (void*) 0xc0090000;
    uint16_t* memparam = (uint16_t*) param_pos;
    sys_params.memsize = 1024*1024 + memparam[0] * 1024 + memparam[1] * 64 * 1024;
}

sys_params_t* get_sysparams()
{
    return &sys_params;
}
