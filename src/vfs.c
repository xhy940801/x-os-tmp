#include "vfs.h"

#include "stddef.h"
#include "string.h"

#include "sched.h"
#include "errno.h"
#include "mem.h"
#include "panic.h"

struct fd_append_info_t
{
    uint16_t fd_pagesize;
    struct fd_struct_t* fd_append;
    int fd_capacity;
    struct level_bitmap_t level_bitmap;
};

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

static inline int _vfs_estimate_capacity(size_t pagesize)
{
    return ((4096 << (pagesize + 1)) - 20) * 31 / (31 * sizeof(struct fd_struct_t) + 4);
}

static inline void fa_alloc_fd_info(struct fd_append_info_t* new_info, size_t pagesize)
{
    new_info->fd_pagesize = pagesize;
    void* newpages = get_pages(pagesize, 1, MEM_FLAGS_P | MEM_FLAGS_K);
    new_info->fd_capacity = _vfs_estimate_capacity(pagesize);
    new_info->fd_append = (struct fd_struct_t*) newpages;
    new_info->level_bitmap.bitmaps = (uint32_t*) (((char*) newpages) + new_info->fd_capacity * sizeof(struct fd_struct_t));
    new_info->level_bitmap.max_level = 4;
}

static inline void fa_cpy_fd_info(struct fd_append_info_t* dst, struct fd_info_t* src)
{
    _memcpy(dst->fd_append, src->fd_append, sizeof(src->fd_append[0]) * src->fd_capacity);
    level_bitmap_cpy(&(dst->level_bitmap), &(src->level_bitmap), src->fd_capacity + 1);
    uint32_t maxtmp = dst->fd_capacity;
    dst->level_bitmap.max_level = 0;
    dst->level_bitmap.bitmaps += 4;
    while(maxtmp >>= 5)
        --dst->level_bitmap.bitmaps;
}

static inline void fa_free_fd_info(struct fd_info_t* fd_info)
{
    free_pages(fd_info->fd_append, fd_info->fd_pagesize);
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
        while(1);
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

void init_fd_info(struct fd_info_t* fd_info)
{
    _memset(fd_info, 0, sizeof(*fd_info) - 4);
    fd_info->fd_size = 0;
    fd_info->fd_capacity = INNER_FD_COUNT;
    fd_info->innerbitmaps = (1 << INNER_FD_COUNT) - 1;
    fd_info->level_bitmap.bitmaps = &(fd_info->innerbitmaps);
}

void release_fd_info(struct fd_info_t* fd_info)
{
    free_pages(fd_info->fd_append, fd_info->fd_pagesize);
}

ssize_t fd_alloc(struct fd_info_t* fd_info)
{
    ssize_t fd = level_bitmap_get_min(&(fd_info->level_bitmap));
    if(fd < 0)
    {
        if(fd_info->fd_size >= fd_info->fd_capacity)
        {
            struct fd_append_info_t fda_info;
            if(fd_info->fd_append == NULL)
            {
                fa_alloc_fd_info(&fda_info, 0);
                fa_cpy_fd_info(&fda_info, fd_info);
            }
            else
            {
                fa_alloc_fd_info(&fda_info, fd_info->fd_pagesize + 1);
                fa_cpy_fd_info(&fda_info, fd_info);
                fa_free_fd_info(fd_info);
            }
            fd_info->fd_pagesize = fda_info.fd_pagesize;
            fd_info->fd_append = fda_info.fd_append;
            fd_info->fd_capacity = fda_info.fd_capacity;
            fd_info->level_bitmap = fda_info.level_bitmap;
        }
        fd = fd_info->fd_size;
        ++fd_info->fd_size;
    }
    kassert(fd >= 0);
    level_bitmap_bit_clear(&(fd_info->level_bitmap), (size_t) fd);
    return fd;
}

void vfs_bind_fd(int fd, uint32_t auth, struct vfs_inode_desc_t* inode, struct fd_info_t* fd_info)
{
    if(fd >= fd_info->fd_capacity)
    {
        int test_page;
        if(fd_info->fd_append == NULL)
            test_page = 0;
        else
            test_page = fd_info->fd_pagesize + 1;
        while(fd >= _vfs_estimate_capacity(test_page))
            ++test_page;
        struct fd_append_info_t fda_info;
        if(fd_info->fd_append == NULL)
        {
            fa_alloc_fd_info(&fda_info, test_page);
            fa_cpy_fd_info(&fda_info, fd_info);
        }
        else
        {
            fa_alloc_fd_info(&fda_info, test_page);
            fa_cpy_fd_info(&fda_info, fd_info);
            fa_free_fd_info(fd_info);
        }
        fd_info->fd_pagesize = fda_info.fd_pagesize;
        fd_info->fd_append = fda_info.fd_append;
        fd_info->fd_capacity = fda_info.fd_capacity;
        fd_info->level_bitmap = fda_info.level_bitmap;
    }
    for(;fd > fd_info->fd_size; ++fd_info->fd_size)
        level_bitmap_bit_set(&fd_info->level_bitmap, (size_t) fd_info->fd_size);
    level_bitmap_bit_clear(&fd_info->level_bitmap, (size_t) fd_info->fd_size);
    ++fd_info->fd_size;
    if(fd < INNER_FD_COUNT)
    {
        fd_info->fds[fd].auth = auth;
        fd_info->fds[fd].inode = inode;
    }
    else
    {
        fd_info->fd_append[fd - INNER_FD_COUNT].auth = auth;
        fd_info->fd_append[fd - INNER_FD_COUNT].inode = inode;
    }
    ++inode->open_count;
}

ssize_t vfs_sys_write(int fd, const char* buf, size_t len)
{
    if(fd < 0 || fd >= cur_process->fd_info.fd_size)
    {
        cur_process->last_errno = EPERM;
        return -1;
    }
    struct fd_struct_t fd_struct;
    if(fd < 16)
        fd_struct = cur_process->fd_info.fds[fd];
    else
        fd_struct = cur_process->fd_info.fd_append[fd - 16];
    if(fd_struct.auth & VFS_FDAUTH_WRITE)
        return vfs_write(fd_struct.inode, buf, len);
    cur_process->last_errno = EPERM;
    return -1;
}

ssize_t vfs_sys_read(int fd, char* buf, size_t len)
{
    if(fd < 0 || fd >= cur_process->fd_info.fd_size)
    {
        cur_process->last_errno = EPERM;
        return -1;
    }
    struct fd_struct_t fd_struct;
    if(fd < 16)
        fd_struct = cur_process->fd_info.fds[fd];
    else
        fd_struct = cur_process->fd_info.fd_append[fd - 16];
    if(fd_struct.auth & VFS_FDAUTH_READ)
        return vfs_read(fd_struct.inode, buf, len);
    cur_process->last_errno = EPERM;
    return -1;
}

int vfs_sys_fsync(int fd)
{
    if(fd < 0 || fd >= cur_process->fd_info.fd_size)
    {
        cur_process->last_errno = EPERM;
        return -1;
    }
    struct vfs_inode_desc_t* inode;
    if(fd < 16)
        inode = cur_process->fd_info.fds[fd].inode;
    else
        inode = cur_process->fd_info.fd_append[fd - 16].inode;
    if(inode == NULL)
    {
        cur_process->last_errno = EPERM;
        return -1;
    }
    return vfs_fsync(inode);
}

int fd_fork(struct process_info_t* dst, struct process_info_t* src)
{
    init_fd_info(&dst->fd_info);
    size_t inner_size = INNER_FD_COUNT < src->fd_info.fd_size ? INNER_FD_COUNT : src->fd_info.fd_size;
    for(size_t i = 0; i < inner_size; ++i)
    {
        if(src->fd_info.fds[i].inode != NULL &&
            !(src->fd_info.fds[i].auth & VFS_FDAUTH_CLOSEONFORK))
            vfs_bind_fd(i, src->fd_info.fds[i].auth, src->fd_info.fds[i].inode, &dst->fd_info);
    }
    for(size_t i = INNER_FD_COUNT; i < src->fd_info.fd_size; ++i)
    {
        if(src->fd_info.fd_append[i - INNER_FD_COUNT].inode != NULL &&
            !(src->fd_info.fd_append[i - INNER_FD_COUNT].auth & VFS_FDAUTH_CLOSEONFORK))
            vfs_bind_fd(i, src->fd_info.fd_append[i - INNER_FD_COUNT].auth, src->fd_info.fd_append[i - INNER_FD_COUNT].inode, &dst->fd_info);
    }
    return 0;
}

int sys_write(int fd, const char* buf, size_t len)
{
    if(((unsigned long) buf) >= KMEM_START)
    {
        cur_process->last_errno = ENOMEM;
        return -1;
    }
    if((KMEM_START - ((unsigned long) buf)) < len)
    {
        cur_process->last_errno = ENOMEM;
        return -1;
    }
    return vfs_sys_write(fd, buf, len);
}

int sys_read(int fd, char* buf, size_t len)
{
    if(((unsigned long) buf) >= KMEM_START)
    {
        cur_process->last_errno = ENOMEM;
        return -1;
    }
    if((KMEM_START - ((unsigned long) buf)) < len)
    {
        cur_process->last_errno = ENOMEM;
        return -1;
    }
    return vfs_sys_read(fd, buf, len);
}


