#pragma once

#include "stdint.h"

#include "vfs.h"

enum
{
    TTY0_STATE_NORMAL   = 0,
    TTY0_STATE_SETTING  = 1
};

struct tty0_vfs_inode_desc_t
{
    struct vfs_inode_desc_t inode_base;
    char color;
    uint32_t state;
    uint32_t tty_base_addr;
};

void init_tty0_module();
struct vfs_inode_desc_t* get_tty0_inode();