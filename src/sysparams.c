#include "stdint.h"
#include "sysparams.h"

static struct sys_params_t sys_params;
extern char _bios_sys_params[];

void init_sysparams()
{
    void* param_pos = (void*) _bios_sys_params;
    uint16_t* memparam = (uint16_t*) param_pos;
    sys_params.memsize = 1024*1024 + memparam[0] * 1024 + memparam[1] * 64 * 1024;
}

struct sys_params_t* get_sysparams()
{
    return &sys_params;
}
