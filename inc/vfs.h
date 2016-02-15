#pragma once

#include "stddef.h"
#include "stdint.h"
#include "types.h"

#include "level_bitmap.h"

#define INNER_FD_COUNT 8

enum
{
    VFS_FDAUTH_EXEC     = 0x0001,
    VFS_FDAUTH_WRITE    = 0x0002,
    VFS_FDAUTH_READ     = 0x0004,
    VFS_FDAUTH_APPEDN   = 0x0010,
    VFS_FDAUTH_CLOSEONFORK  = 0x0100,
};

enum
{
    VFS_TYPE_NONE       = 0,
    VFS_TYPE_TTY0       = 1,
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
    struct vfs_inode_desc_t* inode;
    uint32_t auth;
    uint32_t pos;
};

struct vfs_inode_desc_t
{
    uint32_t fsys_type;
    uint16_t main_driver;
    uint16_t sub_driver;
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
    ssize_t (*read) (struct vfs_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct);
    ssize_t (*write) (struct vfs_inode_desc_t* inode, const char* buf, size_t len, struct fd_struct_t* fd_struct);
    struct vfs_inode_desc_t* (*get_root_inode) (uint16_t main_driver, uint16_t sub_driver);
    int (*fsync) (struct vfs_inode_desc_t* inode, struct fd_struct_t* fd_struct);
};

struct fd_info_t
{
    uint16_t fd_pagesize;
    struct fd_struct_t fds[INNER_FD_COUNT];
    struct fd_struct_t* fd_append;
    int fd_size;
    int fd_capacity;
    uint32_t innerbitmaps;
    struct level_bitmap_t level_bitmap;
};

void init_vfs_module();
int vfs_register(struct vfs_desc_t* vfs, size_t num);

struct vfs_inode_desc_t* vfs_open_child (struct vfs_inode_desc_t* inode, const char* subpath, size_t len);
ssize_t vfs_read (struct vfs_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct);
ssize_t vfs_write (struct vfs_inode_desc_t* inode, const char* buf, size_t len, struct fd_struct_t* fd_struct);
struct vfs_inode_desc_t* vfs_get_root_inode(uint32_t fsys_type, uint16_t main_driver, uint16_t sub_driver);
int vfs_fsync (struct vfs_inode_desc_t* inode, struct fd_struct_t* fd_struct);

void init_fd_info(struct fd_info_t* fd_info);
void release_fd_info(struct fd_info_t* fd_info);

void vfs_bind_fd(int fd, uint32_t auth, struct vfs_inode_desc_t* inode, struct fd_info_t* fd_info);

ssize_t vfs_sys_write(int fd, const char* buf, size_t len);
ssize_t vfs_sys_read(int fd, char* buf, size_t len);
int vfs_sys_fsync(int fd);

struct process_info_t;
int fd_fork(struct process_info_t* dst, struct process_info_t* src);

int sys_write(int fd, const char* buf, size_t len);
int sys_read(int fd, char* buf, size_t len);
