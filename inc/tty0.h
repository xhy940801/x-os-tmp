#pragma once

#include "stdint.h"

#include "vfs.h"

enum
{
    TTY0_STATE_NORMAL   = 0,
    TTY0_STATE_SETTING  = 1, 
    TTY0_STATE_START    = 2,
    
};

void init_tty0_module();