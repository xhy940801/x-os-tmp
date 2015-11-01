#pragma once

#include "stddef.h"
#include "stdint.h"

typedef int32_t off_t;

enum
{
    VFS_TYPE_NONE       = 0,
    VFS_TYPE_XFS        = 2
};

enum
{
    VFS_MDRIVER_HD      = 3,
    VFS_MDRIVER_TTY0    = 5
};

enum
{
    VFS_MOD_UEXEC       = 0100,
    VFS_MOD_UWRITE      = 0200,
    VFS_MOD_UREAD       = 0400,
    VFS_MOD_GEXEC       = 0010,
    VFS_MOD_GWRITE      = 0020,
    VFS_MOD_GREAD       = 0040,
    VFS_MOD_OEXEC       = 0001,
    VFS_MOD_OWRITE      = 0002,
    VFS_MOD_OREAD       = 0004
};

struct vfs_inode_desc_t;

struct fd_struct_t
{
    union
    {
        struct vfs_inode_desc_t* inode;
        int next_free_fd;
    };
};

struct vfs_inode_desc_t
{
    int fsys_type;
    uint16_t main_driver;
    uint16_t sub_driver;
    off_t pos;
    struct vfs_inode_desc_t* parent;
    struct vfs_inode_desc_t* brother;
    struct vfs_inode_desc_t* children;
    uint32_t open_count;
    uint32_t owner_id;
    uint32_t group_id;
    uint32_t flags;
};

struct vfs_desc_t
{
    const char* name;
    struct vfs_inode_desc_t* (*open_child) (struct vfs_inode_desc_t* inode, const char* subpath, size_t len);
    ssize_t (*read) (struct vfs_inode_desc_t* inode, char* buf, size_t len);
    ssize_t (*write) (struct vfs_inode_desc_t* inode, const char* buf, size_t len);
    int (*fsync) (struct vfs_inode_desc_t* inode);
};

void init_vfs_module();
int vfs_register(struct vfs_desc_t* vfs, size_t num);

struct vfs_inode_desc_t* vfs_open_child (struct vfs_inode_desc_t* inode, const char* subpath, size_t len);
ssize_t vfs_read (struct vfs_inode_desc_t* inode, char* buf, size_t len);
ssize_t vfs_write (struct vfs_inode_desc_t* inode, const char* buf, size_t len);
int vfs_fsync (struct vfs_inode_desc_t* inode);

ssize_t sys_write(int fd, const char* buf, size_t len);