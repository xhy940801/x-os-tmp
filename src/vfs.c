#include "vfs.h"

#include "stddef.h"
#include "string.h"

#include "sched.h"
#include "errno.h"

static struct vfs_desc_t* vfs_desc_points[16];

static struct vfs_inode_desc_t* _vfs_default_open_child (struct vfs_inode_desc_t* inode, const char* subpath, size_t len)
{
    cur_process->last_errno = EPERM;
    return NULL;
}

static ssize_t _vfs_default_read (struct vfs_inode_desc_t* inode, char* buf, size_t len)
{
    cur_process->last_errno = EPERM;
    return -1;
}

static ssize_t _vfs_default_write (struct vfs_inode_desc_t* inode, const char* buf, size_t len)
{
    cur_process->last_errno = EPERM;
    return -1;
}

static int _vfs_default_fsync (struct vfs_inode_desc_t* inode)
{
    return 0;
}

void init_vfs_module()
{
    _memset(vfs_desc_points, 0, sizeof(vfs_desc_points));
}

int vfs_register(struct vfs_desc_t* vfs, size_t num)
{
    if(vfs == NULL || vfs_desc_points[num] != NULL)
        return -1;
    if(vfs->open_child == NULL)
        vfs->open_child = _vfs_default_open_child;
    if(vfs->read == NULL)
        vfs->read = _vfs_default_read;
    if(vfs->write == NULL)
        vfs->write = _vfs_default_write;
    if(vfs->fsync == NULL)
        vfs->fsync = _vfs_default_fsync;

    vfs_desc_points[num] = vfs;
    return 0;
}


struct vfs_inode_desc_t* vfs_open_child (struct vfs_inode_desc_t* inode, const char* subpath, size_t len)
{
    struct vfs_desc_t* vfs = vfs_desc_points[inode->main_driver];
    if(vfs == NULL)
    {
        cur_process->last_errno = ENXIO;
        return NULL;
    }
    if(vfs->open_child == NULL)
    {
        cur_process->last_errno = EPERM;
        return NULL;
    }
    return vfs->open_child(inode, subpath, len);
}

ssize_t vfs_read (struct vfs_inode_desc_t* inode, char* buf, size_t len)
{
    struct vfs_desc_t* vfs = vfs_desc_points[inode->main_driver];
    if(vfs == NULL)
    {
        cur_process->last_errno = ENXIO;
        return -1;
    }
    if(vfs->read == NULL)
    {
        cur_process->last_errno = EPERM;
        return -1;
    }
    return vfs->read(inode, buf, len);
}

ssize_t vfs_write (struct vfs_inode_desc_t* inode, const char* buf, size_t len)
{
    struct vfs_desc_t* vfs = vfs_desc_points[inode->main_driver];
    if(vfs == NULL)
    {
        cur_process->last_errno = ENXIO;
        return -1;
    }
    if(vfs->write == NULL)
    {
        cur_process->last_errno = EPERM;
        return -1;
    }
    return vfs->write(inode, buf, len);
}

ssize_t vfs_fsync(struct vfs_inode_desc_t* inode)
{
    struct vfs_desc_t* vfs = vfs_desc_points[inode->main_driver];
    if(vfs == NULL)
    {
        cur_process->last_errno = ENXIO;
        return -1;
    }
    if(vfs->fsync == NULL)
    {
        cur_process->last_errno = EPERM;
        return -1;
    }
    return vfs->fsync(inode);
}


ssize_t sys_write(int fd, const char* buf, size_t len)
{
    if(fd < 0 || fd > cur_process->fd_max)
    {
        cur_process->last_errno = EPERM;
        return -1;
    }
    struct vfs_inode_desc_t* inode;
    if(fd < 16)
        inode = cur_process->fds[fd].inode;
    else
        inode = cur_process->fd_append[fd - 16].inode;
    return vfs_write(inode, buf, len);
}