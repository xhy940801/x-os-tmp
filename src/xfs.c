#include "xfs.h"

#include "stdint.h"

#include "block_buffer.h"
#include "block_driver.h"
#include "vfs.h"
#include "sched.h"
#include "task_locker.h"
#include "panic.h"
#include "circular_list.h"
#include "slab.h"
#include "string.h"
#include "errno.h"

#include "printk.h"

struct xfs_m_inode_desc_t;

struct xfs_hd_super_block_desc_t
{
    uint32_t flags;
    uint32_t block_num;
    uint32_t block_size;
    int32_t inode_head;
};

struct xfs_hd_inode_clause_desc_t
{
    uint32_t inode_count;
    int32_t start_offset;
};

struct xfs_hd_inode_desc_t
{
    uint32_t flags;
    int32_t owner_id;
    int32_t group_id;
    uint32_t size;
    uint32_t created;
    uint32_t modified;
    union
    {
        struct xfs_hd_inode_clause_desc_t clauses[0];
        char data[0];
    };
};

struct xfs_hd_sub_inode_desc_t
{
    uint32_t flags;
    uint32_t reverse;
    struct xfs_hd_inode_clause_desc_t clauses[0];
};

struct xfs_m_super_block_desc_t;

struct xfs_m_inode_desc_t
{
    struct vfs_inode_desc_t inode_base;
    struct block_buffer_weak_pointer_desc_t main_inode;
    struct block_buffer_weak_pointer_desc_t last_used_inode;
    struct block_buffer_weak_pointer_desc_t last_used_block;
    uint32_t last_used_inode_start;
    uint32_t last_used_inode_size;
    uint32_t last_used_block_start;
    uint32_t size;
    struct xfs_m_super_block_desc_t* m_super_block;
};

struct xfs_m_super_block_desc_t
{
    struct list_node_t hash_node;
    uint32_t flags;
    uint32_t block_num;
    uint32_t block_size;
    uint32_t inode_head;
    struct xfs_m_inode_desc_t root_inode;
    union
    {
        struct
        {
            uint16_t sub_driver;
            uint16_t main_driver;
        };
        uint32_t driver_type;
        struct block_buffer_weak_pointer_batch_desc_t bitmaps;  //Must in struct bottom
    };
};

#define SUPER_BLOCK_HASH_TABLE_SIZE 8

static struct list_node_t super_block_hash_table[SUPER_BLOCK_HASH_TABLE_SIZE];
static struct vfs_desc_t xfs_vfs;

static inline struct block_buffer_desc_t* xfs_load_block(uint16_t main_driver, uint16_t sub_driver, uint32_t block_no, int timeout)
{
    long cur_jiffies = jiffies;
    struct block_buffer_desc_t* block_buffer = get_block_buffer(main_driver, sub_driver, block_no, timeout);
    long used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(block_buffer == NULL)
        return NULL;
    cur_jiffies = jiffies;
    if(block_buffer_wait_op_finished(block_buffer, timeout) < 0)
        return NULL;
    kassert(!(block_buffer->flags & BLKBUFFER_FLAG_LOCK));
    used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(!(block_buffer->flags & BLKBUFFER_FLAG_UNSYNC))
        return block_buffer;
    if(sync_block_buffer(block_buffer, timeout) < 0)
        return NULL;
    return block_buffer;
}

static inline uint32_t xfs_build_inode_flags(unsigned oauth, unsigned gauth, unsigned uauth, unsigned bigfile, unsigned inode_level)
{
    uint32_t flags = (oauth << 1);
    flags |= (gauth << 5);
    flags |= (uauth << 9);
    flags |= (bigfile << 16);
    flags |= (inode_level << 24);
    return flags;
}

static inline int xfs_formatting_init_root_inode(struct block_buffer_desc_t* root_inode_buffer)
{
    struct xfs_hd_inode_desc_t* inode = (struct xfs_hd_inode_desc_t*) root_inode_buffer->buffer;
    inode->flags = xfs_build_inode_flags(0, 7, 7, 0, 0);
    inode->owner_id = 0;
    inode->group_id = 0;
    inode->size = 20;
    _memset(inode->data, 0, 20);
    return 0;
}

static inline uint32_t xfs_driver_type_hash(uint32_t driver_type)
{
    return driver_type % SUPER_BLOCK_HASH_TABLE_SIZE;
}

static inline struct block_buffer_desc_t* xfs_block_buffer_weak_pointer_get_without_unsync
        (volatile struct block_buffer_weak_pointer_desc_t* weak_pointer, int timeout)
{
    struct block_buffer_desc_t* buffer = block_buffer_weak_pointer_get((struct block_buffer_weak_pointer_desc_t*) weak_pointer, timeout);
    if(buffer == NULL)
        return NULL;
    if(buffer->flags & BLKBUFFER_FLAG_SYNCING)
    {
        if(block_buffer_wait_op_finished(buffer, 0) < 0)
        {
            release_block_buffer(buffer);
            return NULL;
        }
    }
    if(buffer->flags & BLKBUFFER_FLAG_UNSYNC)
    {
        if(sync_block_buffer(buffer, 0) < 0)
        {
            release_block_buffer(buffer);
            return NULL;
        }
    }
    return buffer;
}

static inline struct block_buffer_desc_t* xfs_block_buffer_weak_pointer_batch_get_without_unsync
        (volatile struct block_buffer_weak_pointer_batch_desc_t* weak_pointer, size_t num, int timeout)
{
    struct block_buffer_desc_t* buffer = block_buffer_weak_pointer_batch_get((struct block_buffer_weak_pointer_batch_desc_t*) weak_pointer, num, timeout);
    if(buffer == NULL)
        return NULL;
    if(buffer->flags & BLKBUFFER_FLAG_SYNCING)
    {
        if(block_buffer_wait_op_finished(buffer, 0) < 0)
        {
            release_block_buffer(buffer);
            return NULL;
        }
    }
    if(buffer->flags & BLKBUFFER_FLAG_UNSYNC)
    {
        if(sync_block_buffer(buffer, 0) < 0)
        {
            release_block_buffer(buffer);
            return NULL;
        }
    }
    return buffer;
}

static inline struct block_buffer_desc_t* xfs_block_buffer_get_without_unsync(uint16_t main_driver, uint16_t sub_driver, uint32_t block_no, int timeout)
{
    struct block_buffer_desc_t* buffer = get_block_buffer(main_driver, sub_driver, block_no, timeout);
    if(buffer == NULL)
        return NULL;
    if(buffer->flags & BLKBUFFER_FLAG_SYNCING)
    {
        if(block_buffer_wait_op_finished(buffer, 0) < 0)
        {
            release_block_buffer(buffer);
            return NULL;
        }
    }
    if(buffer->flags & BLKBUFFER_FLAG_UNSYNC)
    {
        if(sync_block_buffer(buffer, 0) < 0)
        {
            release_block_buffer(buffer);
            return NULL;
        }
    }
    return buffer;
}

static inline struct block_buffer_desc_t* xfs_block_buffer_get_without_syncing(uint16_t main_driver, uint16_t sub_driver, uint32_t block_no, int timeout)
{
    struct block_buffer_desc_t* buffer = get_block_buffer(main_driver, sub_driver, block_no, timeout);
    if(buffer == NULL)
        return NULL;
    if(buffer->flags & BLKBUFFER_FLAG_SYNCING)
    {
        if(block_buffer_wait_op_finished(buffer, 0) < 0)
        {
            release_block_buffer(buffer);
            return NULL;
        }
    }
    return buffer;
}

static inline ssize_t xfs_inode_read_level0(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    struct block_buffer_desc_t* main_inode_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->main_inode, 0);
    if(main_inode_buffer == NULL)
        return -1;
    size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
    if(inode_level != 0)
    {
        release_block_buffer(main_inode_buffer);
        return -2;
    }
    struct xfs_hd_inode_desc_t* hd_inode = (struct xfs_hd_inode_desc_t*) main_inode_buffer->buffer;
    uint32_t size = inode->size;
    len = fd_struct->pos + len > size ? size - len : len;
    len = fd_struct->pos + len > 4096 - sizeof(struct xfs_hd_inode_desc_t) ? 4096 - sizeof(struct xfs_hd_inode_desc_t) : len;
    kassert(len <= 4096);
    if(len == 0)
    {
        release_block_buffer(main_inode_buffer);
        return 0;
    }
    _memcpy(buf, &hd_inode->data[fd_struct->pos], len);
    fd_struct->pos += len;
    release_block_buffer(main_inode_buffer);
    return (ssize_t) len;
}

static inline ssize_t xfs_inode_read_level1(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    if(fd_struct->pos >= inode->size)
        return 0;
    if((fd_struct->pos & (~(4096 - 1))) != inode->last_used_block_start)
    {
        struct block_buffer_desc_t* main_inode_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->main_inode, 0);
        if(main_inode_buffer == NULL)
            return -1;
        size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
        if(inode_level != 1)
        {
            release_block_buffer(main_inode_buffer);
            return -2;
        }
        struct xfs_hd_inode_desc_t* hd_inode = (struct xfs_hd_inode_desc_t*) main_inode_buffer->buffer;
        size_t inode_pos = 0;
        int32_t target_lba;
        for(size_t i = 0; ; ++i)
        {
            if(hd_inode->clauses[i].inode_count == 0 || fd_struct->pos < inode_pos + 4096 * hd_inode->clauses[i].inode_count)
            {
                target_lba = hd_inode->clauses[i].start_offset;
                break;
            }
            inode_pos += 4096 * hd_inode->clauses[i].inode_count;
        }

        target_lba += ((int32_t) main_inode_buffer->block_no) * 4096;
        target_lba += (int32_t) (fd_struct->pos - inode_pos);
        inode->last_used_block_start = fd_struct->pos & (~(4096 - 1));
        inode->last_used_block.block_no = target_lba / 4096;
        release_block_buffer(main_inode_buffer);
    }
    struct block_buffer_desc_t* target_block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->last_used_block, 0);
    if(target_block_buffer == NULL)
        return -1;
    size_t size = inode->size;
    if((fd_struct->pos & (~(4096 - 1))) != inode->last_used_block_start)
    {
        release_block_buffer(target_block_buffer);
        return -2;
    }
    len = fd_struct->pos + len > size ? size - len : len;
    len = (fd_struct->pos + len) /4096 != fd_struct->pos / 4096 ? ((fd_struct->pos + len) & (~(4096 - 1))) - fd_struct->pos : len;
    kassert(len <= 4096 && len != 0);
    char* data = target_block_buffer->buffer;
    _memcpy(buf, &data[fd_struct->pos & (4096 - 1)], len);
    release_block_buffer(target_block_buffer);
    return (ssize_t) len;
}

static inline ssize_t xfs_inode_read_level_other(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    if(fd_struct->pos >= inode->size)
        return 0;
    uint16_t main_driver = inode->inode_base.main_driver;
    uint16_t sub_driver = inode->inode_base.sub_driver;
    if((fd_struct->pos & (~(4096 - 1))) != inode->last_used_block_start)
    {
        if(fd_struct->pos < inode->last_used_inode_start || (inode->last_used_inode_size != 0 && fd_struct->pos >= inode->last_used_inode_start + inode->last_used_inode_size))
        {
            struct block_buffer_desc_t* main_inode_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->main_inode, 0);
            if(main_inode_buffer == NULL)
                return -1;
            size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
            struct xfs_hd_inode_desc_t* hd_inode = (struct xfs_hd_inode_desc_t*) main_inode_buffer->buffer;
            size_t inode_pos = 0;
            size_t inode_size;
            int32_t target_lba;
            for(size_t i = 0; ; ++i)
            {
                inode_size = 4096 * hd_inode->clauses[i].inode_count;
                if(inode_size == 0 || fd_struct->pos < inode_pos + inode_size)
                {
                    target_lba = hd_inode->clauses[i].start_offset;
                    break;
                }
                inode_pos += inode_size;
            }
            target_lba += ((int32_t) main_inode_buffer->block_no) * 4096;
            release_block_buffer(main_inode_buffer);
            --inode_level;
            while(--inode_level)
            {
                struct block_buffer_desc_t* sub_inode_buffer = xfs_block_buffer_get_without_unsync(main_driver, sub_driver, ((uint32_t) target_lba) / 4096, 0);
                if(sub_inode_buffer == NULL)
                    return -1;
                struct xfs_hd_sub_inode_desc_t* hd_sub_inode = (struct xfs_hd_sub_inode_desc_t*) sub_inode_buffer->buffer;
                for(size_t i = 0; ; ++i)
                {
                    inode_size = 4096 * hd_sub_inode->clauses[i].inode_count;
                    if(inode_size == 0 || fd_struct->pos < inode_pos + inode_size)
                    {
                        target_lba += hd_sub_inode->clauses[i].start_offset;
                        break;
                    }
                    inode_pos += inode_size;
                }
                release_block_buffer(sub_inode_buffer);
            }
            inode->last_used_inode_start = inode_pos;
            inode->last_used_inode_size = inode_size;
            inode->last_used_inode.block_no = ((uint32_t) target_lba) / 4096;
        }
        uint32_t last_used_inode_start = inode->last_used_inode_start;
        uint32_t last_used_inode_size = inode->last_used_inode_size;
        struct block_buffer_desc_t* inode_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->last_used_inode, 0);
        if(inode_buffer == NULL)
            return -1;
        kassert(!(fd_struct->pos < last_used_inode_start || (last_used_inode_size != 0 && fd_struct->pos >= last_used_inode_start + last_used_inode_size)));
        struct xfs_hd_sub_inode_desc_t* hd_inode = (struct xfs_hd_sub_inode_desc_t*) inode_buffer->buffer;
        size_t inode_pos = last_used_inode_start;
        int32_t target_lba;
        for(size_t i = 0; ; ++i)
        {
            if(hd_inode->clauses[i].inode_count == 0 || fd_struct->pos < inode_pos + 4096 * hd_inode->clauses[i].inode_count)
            {
                target_lba = hd_inode->clauses[i].start_offset;
                break;
            }
            inode_pos += 4096 * hd_inode->clauses[i].inode_count;
        }
        target_lba += ((int32_t) inode_buffer->block_no) * 4096;
        target_lba += (int32_t) (fd_struct->pos - inode_pos);
        inode->last_used_block_start = fd_struct->pos & (~(4096 - 1));
        inode->last_used_block.block_no = target_lba / 4096;
        release_block_buffer(inode_buffer);
    }
    size_t last_used_block_start = inode->last_used_block_start;
    struct block_buffer_desc_t* target_block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->last_used_block, 0);
    size_t size = inode->size;
    kassert((fd_struct->pos & (~(4096 - 1))) == last_used_block_start);
    len = fd_struct->pos + len > size ? size - len : len;
    len = (fd_struct->pos + len) /4096 != fd_struct->pos / 4096 ? ((fd_struct->pos + len) & (~(4096 - 1))) - fd_struct->pos : len;
    kassert(len <= 4096 && len != 0);
    char* data = target_block_buffer->buffer;
    _memcpy(buf, &data[fd_struct->pos & (4096 - 1)], len);
    release_block_buffer(target_block_buffer);
    return (ssize_t) len;
}

static inline ssize_t xfs_inode_read_once(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
    if(inode_level == 0)
        return xfs_inode_read_level0(inode, buf, len, fd_struct);
    else if(inode_level == 1)
        return xfs_inode_read_level1(inode, buf, len, fd_struct);
    else
        return xfs_inode_read_level_other(inode, buf, len, fd_struct);
}

ssize_t xfs_inode_read(struct vfs_inode_desc_t* _inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    volatile struct xfs_m_inode_desc_t* inode = parentof(_inode, struct xfs_m_inode_desc_t, inode_base);
    kassert(inode->inode_base.fsys_type == VFS_TYPE_XFS);
    size_t has_read = 0;
    while(has_read < len)
    {
        lock_task();
        ssize_t res = xfs_inode_read_once(inode, buf + has_read, len - has_read, fd_struct);
        unlock_task();
        kassert(res >=0 || res == -1 || res == -2);
        if(res > 0)
            has_read += (size_t) res;
        else if(res == 0 || has_read > 0)
            return (ssize_t) has_read;
        else if(res == -2)
            continue;
        else
        {
            cur_process->last_errno = cur_process->sub_errno;
            return -1;
        }
    }
    return (ssize_t) has_read;
}

static inline struct block_buffer_desc_t* xfs_malloc_one_inode_block(struct xfs_m_super_block_desc_t* m_super_block, uint16_t main_driver, uint16_t sub_driver)
{
    size_t bitmap_index;
    size_t bitmap_index_max = (m_super_block->block_num + 4096 * 8 - 1) / (4096 * 8);
    for(bitmap_index = 0; bitmap_index < bitmap_index_max - 1; ++bitmap_index)
    {
        struct block_buffer_desc_t* bitmap = xfs_block_buffer_weak_pointer_batch_get_without_unsync(&m_super_block->bitmaps, bitmap_index, 0);
        if(bitmap == NULL)
            return NULL;
        uint8_t* data = (uint8_t*) bitmap->buffer;
        for(size_t i = 0; i < 4096; ++i)
        {
            int flags;
            uint32_t pos = _bsfc(~data[i], flags);
            if(flags & PSW_ZF)
            {
                uint8_t mask = (0x01 >> pos);
                data[i] |= mask;
                uint32_t block_no = bitmap_index * 4096 * 8 + i * 8 + pos;
                bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
                struct block_buffer_desc_t* block_buffer = xfs_block_buffer_get_without_syncing(main_driver, sub_driver, block_no, 0);
                if(block_buffer == NULL)
                {
                    data[i] &= ~mask;
                    release_block_buffer(bitmap);
                    return NULL;
                }
                release_block_buffer(bitmap);
                return block_buffer;
            }
        }
        release_block_buffer(bitmap);
    }
    {
        struct block_buffer_desc_t* bitmap = xfs_block_buffer_weak_pointer_batch_get_without_unsync(&m_super_block->bitmaps, bitmap_index, 0);
        if(bitmap == NULL)
            return NULL;
        uint8_t* data = (uint8_t*) bitmap->buffer;
        size_t len = m_super_block->block_num % (4096 * 8);
        for(size_t i = 0; i < len; ++i)
        {
            uint8_t mask = 0x01 << (i % 8);
            if(data[i] & mask)
                continue;
            data[i] |= mask;
            uint32_t block_no = bitmap_index * 4096 * 8 + i;
            bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
            struct block_buffer_desc_t* block_buffer = xfs_block_buffer_get_without_syncing(main_driver, sub_driver, block_no, 0);
            if(block_buffer == NULL)
            {
                data[i] &= ~mask;
                release_block_buffer(bitmap);
                return NULL;
            }
            release_block_buffer(bitmap);
            return block_buffer;
        }
        release_block_buffer(bitmap);
    }
    cur_process->last_errno = ENOSPC;
    return NULL;
}

static inline struct block_buffer_desc_t* xfs_malloc_one_data_block(struct xfs_m_super_block_desc_t* m_super_block, uint16_t main_driver, uint16_t sub_driver)
{
    size_t bitmap_index = (m_super_block->block_num + 4096 * 8 - 1) / (4096 * 8);
    kassert(bitmap_index > 0);
    --bitmap_index;
    {
        struct block_buffer_desc_t* bitmap = xfs_block_buffer_weak_pointer_batch_get_without_unsync(&m_super_block->bitmaps, bitmap_index, 0);
        if(bitmap == NULL)
            return NULL;
        uint8_t* data = (uint8_t*) bitmap->buffer;
        size_t i = (m_super_block->block_num / 8) % 4096;
        while(i--)
        {
            if(data[i] == 0)
            {
                data[i] |= 0x01;
                uint32_t block_no = bitmap_index * 4096 * 8 + i * 8;
                bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
                struct block_buffer_desc_t* block_buffer = xfs_block_buffer_get_without_syncing(main_driver, sub_driver, block_no, 0);
                if(block_buffer == NULL)
                {
                    data[i] &= 0xfe;
                    release_block_buffer(bitmap);
                    return NULL;
                }
                release_block_buffer(bitmap);
                return block_buffer;
            }
        }
    }
    while(bitmap_index--)
    {
        struct block_buffer_desc_t* bitmap = xfs_block_buffer_weak_pointer_batch_get_without_unsync(&m_super_block->bitmaps, bitmap_index, 0);
        if(bitmap == NULL)
            return NULL;
        uint8_t* data = (uint8_t*) bitmap->buffer;
        size_t i = 4096;
        while(i--)
        {
            if(data[i] == 0)
            {
                data[i] |= 0x01;
                uint32_t block_no = bitmap_index * 4096 * 8 + i * 8;
                bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
                struct block_buffer_desc_t* block_buffer = xfs_block_buffer_get_without_syncing(main_driver, sub_driver, block_no, 0);
                if(block_buffer == NULL)
                {
                    data[i] &= 0xfe;
                    release_block_buffer(bitmap);
                    return NULL;
                }
                release_block_buffer(bitmap);
                return block_buffer;
            }
        }
    }
    bitmap_index = (m_super_block->block_num + 4096 * 8 - 1) / (4096 * 8);
    --bitmap_index;
    {
        struct block_buffer_desc_t* bitmap = xfs_block_buffer_weak_pointer_batch_get_without_unsync(&m_super_block->bitmaps, bitmap_index, 0);
        if(bitmap == NULL)
            return NULL;
        uint8_t* data = (uint8_t*) bitmap->buffer;
        size_t i = m_super_block->block_num % (4096 * 8);
        while(i--)
        {
            size_t pos = i / 8;
            uint8_t mask = 0x01 << (i % 8);
            if((data[pos] & mask) == 0)
            {
                data[pos] |= mask;
                uint32_t block_no = bitmap_index * 4096 * 8 + i;
                bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
                struct block_buffer_desc_t* block_buffer = xfs_block_buffer_get_without_syncing(main_driver, sub_driver, block_no, 0);
                if(block_buffer == NULL)
                {
                    data[pos] &= (~mask);
                    release_block_buffer(bitmap);
                    return NULL;
                }
                release_block_buffer(bitmap);
                return block_buffer;
            }
        }
    }
    while(bitmap_index--)
    {
        struct block_buffer_desc_t* bitmap = xfs_block_buffer_weak_pointer_batch_get_without_unsync(&m_super_block->bitmaps, bitmap_index, 0);
        if(bitmap == NULL)
            return NULL;
        uint8_t* data = (uint8_t*) bitmap->buffer;
        size_t i = 4096 * 8;
        while(i--)
        {
            size_t pos = i / 8;
            uint8_t mask = 0x01 << (i % 8);
            if((data[pos] & mask) == 0)
            {
                data[pos] |= mask;
                uint32_t block_no = bitmap_index * 4096 * 8 + i;
                bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
                struct block_buffer_desc_t* block_buffer = xfs_block_buffer_get_without_syncing(main_driver, sub_driver, block_no, 0);
                if(block_buffer == NULL)
                {
                    data[pos] &= (~mask);
                    release_block_buffer(bitmap);
                    return NULL;
                }
                release_block_buffer(bitmap);
                return block_buffer;
            }
        }
    }
    cur_process->last_errno = ENOSPC;
    return NULL;
}

static inline struct block_buffer_desc_t* xfs_try_append_one_data_block(
    struct xfs_m_super_block_desc_t* m_super_block, uint16_t main_driver, uint16_t sub_driver, uint32_t block_no, int* err)
{
    ++block_no;
    *err = 1;
    size_t bitmap_index = block_no / (4096 * 8);
    struct block_buffer_desc_t* bitmap = xfs_block_buffer_weak_pointer_batch_get_without_unsync(&m_super_block->bitmaps, bitmap_index, 0);
    if(bitmap == NULL)
        return NULL;
    uint8_t mask = 0x01 << (block_no % 8);
    size_t pos = (block_no / 8) % 4096;
    uint8_t* data = (uint8_t*) bitmap->buffer;
    if(data[pos] & mask == 0)
    {
        struct block_buffer_desc_t* block_buffer = xfs_block_buffer_get_without_syncing(main_driver, sub_driver, block_no, 0);
        if(block_buffer == NULL)
        {
            data[pos] &= (~mask);
            release_block_buffer(bitmap);
            return NULL;
        }
        if(data[pos] & mask == 0)
        {
            data[pos] |= mask;
            bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
            release_block_buffer(bitmap);
            return block_buffer;
        }
    }
    *err = 0;
    return NULL;
}

static inline int xfs_free_one_block(struct xfs_m_super_block_desc_t* m_super_block, struct block_buffer_desc_t* block_buffer)
{
    size_t bitmap_index = block_buffer->block_no / (4096 * 8);
    struct block_buffer_desc_t* bitmap = xfs_block_buffer_weak_pointer_batch_get_without_unsync(&m_super_block->bitmaps, bitmap_index, 0);
    if(bitmap == NULL)
        return -1;
    size_t i = m_super_block->block_num % (4096 * 8);
    size_t pos = i / 8;
    uint8_t mask = 0x01 << (i % 8);
    uint8_t data = (uint8_t*) bitmap->buffer;
    data[pos] &= (~mask);
    release_block_buffer(bitmap);
    return 0;
}

static inline ssize_t xfs_inode_append_write_append_inode(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, size_t level, uint32_t* target_lba)
{
    size_t pos = inode->size;
    struct xfs_m_super_block_desc_t* m_super_block = inode->m_super_block;
    uint16_t main_driver = inode->main_driver;
    uint16_t sub_driver = inode->sub_driver;
    struct block_buffer_desc_t* data_block_buffer = xfs_malloc_one_data_block(m_super_block, main_driver, sub_driver);
    if(data_block_buffer == NULL)
    {
        xfs_free_one_block(m_super_block, data_block_buffer);
        return -1;
    }
    struct block_buffer_desc_t* last_block_buffer = data_block_buffer;
    struct block_buffer_desc_t* inode_block_buffers[level];
    size_t i;
    for(i = 0; i < level; ++i)
    {
        if(pos != inode->size)
            break;
        inode_block_buffers[i] = xfs_malloc_one_inode_block(m_super_block, main_driver, sub_driver);
        if(inode_block_buffers[i] == NULL)
            break;
    }
    if(i != level || pos != inode->size)
    {
        for(size_t j = 0; j < i; ++j)
            xfs_free_one_block(m_super_block, inode_block_buffers[i]);
        xfs_free_one_block(m_super_block, data_block_buffer);
        return i != level ? -1 : -2;
    }
    struct block_buffer_desc_t* last_block_buffer = data_block_buffer;
    for(size_t i = 0; i < level; ++i)
    {
        struct xfs_hd_sub_inode_desc_t* hd_sub_inode = (struct xfs_hd_sub_inode_desc_t*) inode_block_buffers[i]->buffer;
        hd_sub_inode->flags = 0x01;
        hd_sub_inode->reverse = 0x00;
        hd_sub_inode->clauses[0].inode_count = 0;
        int32_t slba = (int32_t) (inode_block_buffers[i]->block_no * 4096);
        int32_t tlba = (int32_t) (last_block_buffer->block_no * 4096);
        hd_sub_inode->clauses[0].start_offset = tlba - slba;
        inode_block_buffers[i]->flags |= BLKBUFFER_FLAG_DIRTY;
        last_block_buffer = inode_block_buffers[i];
    }
    len = len > 4096 ? 4096 : len;
    _memcpy(data_block_buffer->buffer, buf, len);
    data_block_buffer->flags |= BLKBUFFER_FLAG_DIRTY;
    inode->size += len;
    target_lba = inode_block_buffers[level - 1]->block_no;
    return (ssize_t) len;
}

static inline struct block_buffer_desc_t* xfs_inode_write_uplevel(volatile struct xfs_m_inode_desc_t* inode)
{
    struct xfs_m_super_block_desc_t* m_super_block = inode->m_super_block;
    struct block_buffer_desc_t* main_inode_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->main_inode, 0);
    if(main_inode_buffer == NULL)
        return NULL;
    size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
    uint16_t main_driver = inode->main_driver;
    uint16_t sub_driver = inode->sub_driver;
    struct block_buffer_desc_t* block_buffer;
    if(inode_level == 0)
        block_buffer = xfs_malloc_one_data_block(m_super_block, main_driver, sub_driver);
    else
        block_buffer = xfs_malloc_one_inode_block(m_super_block, main_driver, sub_driver);
    if(block_buffer == NULL)
    {
        release_block_buffer(main_inode_buffer);
        return NULL;
    }
    size_t _inode_level = (inode->inode_base.flags << 24) & 0x0f;
    if(inode_level != _inode_level)
    {
        int res = xfs_free_one_block(m_super_block, block_buffer);
        kassert(res >= 0);
        release_block_buffer(block_buffer);
        release_block_buffer(main_inode_buffer);
        return NULL;
    }
    struct xfs_hd_inode_desc_t* hd_inode = (struct xfs_hd_inode_desc_t*) main_inode_buffer->buffer;
    char* data = (char*) block_buffer->buffer;
    if(inode_level == 0)
        _memcpy(data, hd_inode->data, inode->size);
    else
    {
        struct xfs_hd_sub_inode_desc_t* hd_sub_inode = (struct xfs_hd_sub_inode_desc_t*) data;
        hd_sub_inode->flags = 0x01;
        hd_sub_inode->reverse = 0x00;
        _memcpy(hd_sub_inode->clauses, hd_inode->clauses, 4096 - offsetof(hd_inode->clauses));
    }
    hd_inode->clauses[0].inode_count = 0;
    hd_inode->clauses[0].start_offset = (int32_t) (block_buffer->block_no * 4096);
    hd_inode->clauses[0].start_offset -= (int32_t) (main_inode_buffer->block_no * 4096);
    hd_inode->flags = (hd_inode->flags & 0xffffff0f) | ((inode_level + 1) >> 24);
    inode->inode_base.flags = (inode->inode_base.flags & 0xffffff0f) | ((inode_level + 1) >> 24);
    block_buffer->flags &= (^BLKBUFFER_FLAG_UNSYNC);
    block_buffer->flags |= BLKBUFFER_FLAG_DIRTY;
    main_inode_buffer->flags |= BLKBUFFER_FLAG_DIRTY;
    release_block_buffer(main_inode_buffer);
    return block_buffer;
}

static ssize_t xfs_inode_append_write_level(
    volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct, struct block_buffer_desc_t* block_buffer,
    uint32_t inode_count, size_t level, size_t* need_append)
{
    size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
    kassert(inode_level > level);
    kassert(inode_level != 0);
    uint16_t main_driver = inode->inode_base.main_driver;
    uint16_t sub_driver = inode->inode_base.sub_driver;
    if(level == 0)
    {
        size_t pos = inode->size;
        uint8_t* data = (uint8_t*) block_buffer->buffer;
        block_buffer->flags |= BLBUFFER_FLAG_DIRTY;
        ssize_t rs;
        size_t llen = 4096 - (pos % 4096);
        if(len > llen)
        {
            _memcpy(data + pos % 4096, buf, llen);
            inode->size += llen;
            struct xfs_m_super_block_desc_t* m_super_block = inode->m_super_block;
            int err;
            pos += llen;
            kassert(pos == inode->size && pos % 4096 == 0);
            struct block_buffer_desc_t* nxt_block_buffer = xfs_try_append_one_data_block(m_super_block, main_driver, sub_driver, block_buffer->block_no, &err);
            if(nxt_block_buffer == NULL)
            {
                if(*err)
                    return -1;
                if(inode->size != pos)
                    *need_append = 0;
                return (ssize_t) llen;
            }
            else
            {
                *need_append = 0;
                if(pos != inode->size)
                {
                    xfs_free_one_block(m_super_block, nxt_block_buffer);
                    return (ssize_t) llen;
                }
                buf += llen;
                len -= llen;
                data = (uint8_t*) nxt_block_buffer->buffer;
                len = len > 4096 ? 4096 : len;
                _memcpy(data, buf, len);
                release_block_buffer(nxt_block_buffer);
                return (ssize_t) (llen + len);
            }
        }
        else
        {
            _memcpy(data + pos % 4096, buf, len);
            need_append = 0;
            inode->size += len;
            return (ssize_t) len;
        }
    }
    else
    {
        struct xfs_hd_sub_inode_desc_t* hd_sub_inode = (struct xfs_hd_sub_inode_desc_t*) block_buffer->buffer;
        kassert(hd_sub_inode->flags == 0x01);
        kassert(hd_sub_inode->reverse == 0x00);
        size_t i = 0;
        const size_t clause_len = (4096 - offsetof(clauses, struct xfs_hd_sub_inode_desc_t)) / sizeof(struct xfs_hd_inode_clause_desc_t);
        uint32_t _inode_count = inode_count;
        while(i < clause_len && hd_sub_inode->clauses[i].inode_count != 0)
        {
            inode_count += hd_sub_inode->clauses[i].inode_count;
            ++i;
        }
        kassert(i <= clause_len);
        if(i == clause_len)
        {
            release_block_buffer(block_buffer);
            return -2;
        }
        if(hd_sub_inode->clauses[i].start_offset == 0)
        {
            uint32_t target_lba;
            ssize_t rs = xfs_inode_append_write_append_inode(inode, buf, len, level, target_lba);
            if(rs > 0)
            {
                int32_t slba = (int32_t) (block_buffer->block_no * 4096);
                int32_t tlba = (int32_t) target_lba;
                hd_sub_inode->clauses[i].start_offset = tlba - slba;
            }
            release_block_buffer(block_buffer);
            return -2;
        }
        int32_t target_lba = ((int32_t) block_buffer->block_no) * 4096 + hd_sub_inode->clauses[i].start_offset;
        kassert(target_lba % 4096 == 0);
        uint32_t block_no = target_lba / 4096;
        size_t rs;
        struct block_buffer_desc_t* _block_buffer;
        if(level == 1)
        {
            inode->last_used_block.block_no = block_no + (pos / 4096 - inode_count);
            *need_append = (pos / 4096 - inode_count) + 1;
            inode->last_used_block_start = pos & (~(4096 - 1));
            _block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->last_used_block, 0);
        }
        else if(level == 2)
        {
            inode->last_used_inode.block_no = block_no;
            inode->last_used_inode_start = inode_count * 4096;
            inode->last_used_inode_size = 0;
            _block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->last_used_inode, 0);
        }
        else
            _block_buffer = xfs_block_buffer_get_without_unsync(main_driver, sub_driver, block_no);
        if(_block_buffer == NULL)
        {
            release_block_buffer(block_buffer);
            return -1;
        }
        if(hd_sub_inode->clauses[i].inode_count != 0)
        {
            release_block_buffer(_block_buffer);
            release_block_buffer(block_buffer);
            return -2;
        }
        ssize_t rs = xfs_inode_append_write_level(inode, buf, len, fd_struct, _block_buffer, inode_count, level - 1, need_append);
        release_block_buffer(_block_buffer);
        kassert(rs != 0);
        if(rs < 0 || *need_append == 0 || ((size_t) rs) == len)
        {
            release_block_buffer(block_buffer);
            return rs;
        }
        hd_sub_inode->clauses[i].inode_count = *need_append;
        ++i;
        kassert(i <= clause_len);
        if(i == clause_len)
        {
            *need_append += inode_count - _inode_count;
            release_block_buffer(block_buffer);
            return rs;
        }
        *need_append = 0;
        hd_sub_inode->clauses[i].start_offset = 0;
        hd_sub_inode->clauses[i].inode_count = 0;
        kassert(rs > 0);
        uint32_t target_lba;
        ssize_t rs2 = xfs_inode_append_write_append_inode(inode, buf + rs; len - (size_t) rs, level, &target_lba);
        if(rs2 > 0)
        {
            int32_t slba = (int32_t) (block_buffer->block_no * 4096);
            int32_t tlba = (int32_t) target_lba;
            hd_sub_inode->clauses[i].start_offset = tlba - slba;
            return rs + rs2;
        }
        return rs;
    }
}

static inline ssize_t xfs_inode_append_write_at_level0(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
    if(inode_level == 0)
    {
        struct block_buffer_desc_t* block_buffer*;
        size_t pos = inode->size;
        if(pos + len > 4096 - offsetof(data, struct xfs_hd_inode_desc_t))
        {
            block_buffer = xfs_inode_write_uplevel(inode);
            if(block_buffer == NULL)
                return -1;
            pos = inode->size;
            len = len > 4096 - pos ? 4096 - pos : len;
        }
        else
        {
            block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->main_inode, 0);
            if(block_buffer == NULL)
                return -1;
            inode_level = (inode->inode_base.flags << 24) & 0x0f;
            pos = inode->size;
            if(inode_level != 0 || pos + len > 4096 - offsetof(data, struct xfs_hd_inode_desc_t))
            {
                release_block_buffer(block_buffer);
                return -2;
            }
        }
        uint8_t* data = (uint8_t*) block_buffer->buffer;
        _memcpy(data + pos, buf, len);
        block_buffer->flags |= BLKBUFFER_FLAG_DIRTY;
        release_block_buffer(block_buffer);
        return len;
    }
    struct block_buffer_desc_t* block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->main_inode, 0);
    inode_level = (inode->inode_base.flags << 24) & 0x0f;
    struct xfs_hd_inode_desc_t* hd_inode = (struct xfs_hd_inode_desc_t*) block_buffer->buffer;
    const size_t clause_len = (4096 - offsetof(clauses, struct xfs_hd_sub_inode_desc_t)) / sizeof(struct xfs_hd_inode_clause_desc_t);
    size_t inode_count = 0;
    while(i < clause_len && hd_inode->clauses[i].inode_count != 0)
    {
        inode_count += hd_inode->clauses[i].inode_count;
        ++i;
    }
    kassert(i <= clause_len);
    if(i == clause_len)
    {
        release_block_buffer(block_buffer);
        return -2;
    }
    if(hd_inode->clauses[i].start_offset == 0)
    {
        uint32_t target_lba;
        ssize_t rs = xfs_inode_append_write_append_inode(inode, buf, len, inode_level, target_lba);
        if(rs > 0)
        {
            int32_t slba = (int32_t) (block_buffer->block_no * 4096);
            int32_t tlba = (int32_t) target_lba;
            hd_inode->clauses[i].start_offset = tlba - slba;
        }
        release_block_buffer(block_buffer);
        return -2;
    }
    int32_t target_lba = ((int32_t) block_buffer->block_no) * 4096 + hd_inode->clauses[i].start_offset;
    kassert(target_lba % 4096 == 0);
    uint32_t block_no = target_lba / 4096;
    size_t rs;
    struct block_buffer_desc_t* _block_buffer;
    if(inode_level == 1)
    {
        inode->last_used_block.block_no = block_no + (pos / 4096 - inode_count);
        *need_append = (pos / 4096 - inode_count) + 1;
        inode->last_used_block_start = pos & (~(4096 - 1));
        _block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->last_used_block, 0);
    }
    else if(inode_level == 2)
    {
        inode->last_used_inode.block_no = block_no;
        inode->last_used_inode_start = inode_count * 4096;
        inode->last_used_inode_size = 0;
        _block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->last_used_inode, 0);
    }
    else
        _block_buffer = xfs_block_buffer_get_without_unsync(main_driver, sub_driver, block_no);
    if(_block_buffer == NULL)
    {
        release_block_buffer(block_buffer);
        return -1;
    }
    if(hd_inode->clauses[i].inode_count != 0)
    {
        release_block_buffer(_block_buffer);
        release_block_buffer(block_buffer);
        return -2;
    }
    size_t need_append;
    ssize_t rs = xfs_inode_append_write_level(inode, buf, len, fd_struct, _block_buffer, inode_count, inode_level - 1, &need_append);
    release_block_buffer(_block_buffer);
    kassert(rs != 0);
    if(rs < 0 || need_append == 0 || ((size_t) rs) == len)
    {
        release_block_buffer(block_buffer);
        return rs;
    }
    hd_inode->clauses[i].inode_count = need_append;
    ++i;
    kassert(i <= clause_len);
    if(i == clause_len)
    {
        release_block_buffer(xfs_inode_write_uplevel(inode));
        release_block_buffer(block_buffer);
        return rs;
    }
    hd_inode->clauses[i].start_offset = 0;
    hd_inode->clauses[i].inode_count = 0;
    kassert(rs > 0);
    uint32_t target_lba;
    ssize_t rs2 = xfs_inode_append_write_append_inode(inode, buf + rs; len - (size_t) rs, level, &target_lba);
    if(rs2 > 0)
    {
        int32_t slba = (int32_t) (block_buffer->block_no * 4096);
        int32_t tlba = (int32_t) target_lba;
        hd_inode->clauses[i].start_offset = tlba - slba;
        return rs + rs2;
    }
    return rs;
}

static inline ssize_t xfs_inode_append_write_once(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
    if(inode_level == 0)
    {
        if(inode->size + len > 4096 - sizeof(struct xfs_hd_inode_desc_t))
            return xfs_inode_uplevel_from_level0(inode, buf, len, fd_struct);
        return xfs_inode_append_write_level0(inode, buf, len, fd_struct);
    }
    else if(inode_level == 1)
        return xfs_inode_append_write_level1(inode, buf, len, fd_struct);
    else
        return xfs_inode_append_write_level_other(inode, buf, len, fd_struct);
}

static inline ssize_t xfs_inode_append_write(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    size_t has_write = 0;
    while(has_write < len)
    {
        lock_task();
        ssize_t res = xfs_inode_append_write_once(inode, buf + has_write, len - has_write, fd_struct);
        unlock_task();
        kassert(res >=0 || res == -1 || res == -2);
        if(res > 0)
            has_write += (size_t) res;
        else if(res == 0 || has_write > 0)
            return (ssize_t) has_write;
        else if(res == -2)
            continue;
        else
        {
            cur_process->last_errno = cur_process->sub_errno;
            return -1;
        }
    }
    return (ssize_t) has_write;
}

static inline ssize_t xfs_inode_normal_write(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
}

ssize_t xfs_inode_write(struct vfs_inode_desc_t* _inode, char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    volatile struct xfs_m_inode_desc_t* inode = parentof(_inode, struct xfs_m_inode_desc_t, inode_base);
    kassert(inode->inode_base.fsys_type == VFS_TYPE_XFS);
    if(fd_struct->auth & VFS_FDAUTH_APPEDN)
    {
        fd_struct->pos = inode->size;
        return xfs_inode_append_write(inode, buf, len, fd_struct);
    }
    else
        return xfs_inode_normal_write(inode, buf, len, fd_struct);
}

struct vfs_inode_desc_t* xfs_get_root_inode(uint16_t main_driver, uint16_t sub_driver)
{
    uint32_t driver_type = sub_driver | main_driver << 16;
    size_t hash = xfs_driver_type_hash(driver_type);
    struct list_node_t* node = super_block_hash_table[hash].next;
    struct xfs_m_super_block_desc_t* m_super_block = NULL;
    lock_task();
    while(node != &super_block_hash_table[hash])
    {
        struct xfs_m_super_block_desc_t* super_block = parentof(node, struct xfs_m_super_block_desc_t, hash_node);
        if(super_block->driver_type == driver_type)
        {
            m_super_block = super_block;
            break;
        }
        node = node->next;
    }
    if(m_super_block == NULL)
    {
        cur_process->last_errno = ENODEV;
        unlock_task();
        return NULL;
    }
    unlock_task();
    return &m_super_block->root_inode.inode_base;
}

void init_xfs_module()
{
    for(size_t i = 0; i < SUPER_BLOCK_HASH_TABLE_SIZE; ++i)
        circular_list_init(&super_block_hash_table[i]);
    xfs_vfs.read = xfs_inode_read;
    xfs_vfs.get_root_inode = xfs_get_root_inode;
    vfs_register(&xfs_vfs, VFS_TYPE_XFS);
    printk("xfs_m_inode size [%u]\n", sizeof(struct xfs_m_inode_desc_t));
}

int load_xfs_inode(uint16_t main_driver, uint16_t sub_driver, uint32_t block_no, struct xfs_m_inode_desc_t* m_inode, int timeout)
{
    lock_task();
    struct block_buffer_desc_t* inode_block_buffer = xfs_load_block(main_driver, sub_driver, block_no, timeout);
    if(inode_block_buffer == NULL)
    {
        unlock_task();
        return -1;
    }
    struct xfs_hd_inode_desc_t* hd_inode = (struct xfs_hd_inode_desc_t*) inode_block_buffer->buffer;
    m_inode->inode_base.fsys_type = VFS_TYPE_XFS;
    m_inode->inode_base.main_driver = main_driver;
    m_inode->inode_base.sub_driver = sub_driver;
    m_inode->inode_base.owner_id = hd_inode->owner_id;
    m_inode->inode_base.group_id = hd_inode->group_id;
    m_inode->inode_base.flags = hd_inode->flags;
    m_inode->size = hd_inode->size;
    m_inode->main_inode.main_driver = main_driver;
    m_inode->main_inode.sub_driver = sub_driver;
    m_inode->main_inode.block_no = block_no;
    m_inode->main_inode.block_buffer = inode_block_buffer;
    m_inode->last_used_inode = m_inode->main_inode;
    unlock_task();
    return 0;
}

int load_xfs(uint16_t main_driver, uint16_t sub_driver, int timeout)
{
    uint32_t driver_type = sub_driver | main_driver << 16;
    size_t hash = xfs_driver_type_hash(driver_type);
    struct list_node_t* node = super_block_hash_table[hash].next;
    lock_task();
    while(node != &super_block_hash_table[hash])
    {
        struct xfs_m_super_block_desc_t* super_block = parentof(node, struct xfs_m_super_block_desc_t, hash_node);
        if(super_block->driver_type == driver_type)
        {
            unlock_task();
            return 0;
        }
        node = node->next;
    }

    long cur_jiffies = jiffies;
    struct block_buffer_desc_t* super_block_buffer = xfs_load_block(main_driver, sub_driver, 0, timeout);
    long used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(super_block_buffer == NULL)
    {
        unlock_task();
        return -1;
    }

    struct xfs_hd_super_block_desc_t* hd_super_block = (struct xfs_hd_super_block_desc_t*) super_block_buffer->buffer;
    if(hd_super_block->flags != XFS_MAGIC_NO || hd_super_block->block_size != 4096)
    {
        release_block_buffer(super_block_buffer);
        unlock_task();
        return -1;
    }
    size_t bitmap_block_count = (hd_super_block->block_num + 4096 * 8 - 1) / (4096 * 8);
    size_t msp_size = (sizeof(struct xfs_m_super_block_desc_t) + sizeof(struct block_buffer_desc_t*) * bitmap_block_count + 0x0f) & (~0xf);
    kassert(msp_size <= 128);
    struct xfs_m_super_block_desc_t* m_super_block = tslab_malloc(msp_size);
    _memset(m_super_block, 0, msp_size);
    m_super_block->flags = hd_super_block->flags;
    m_super_block->block_num = hd_super_block->block_num;
    m_super_block->block_size = hd_super_block->block_size;
    m_super_block->inode_head = (uint32_t) (0 + hd_super_block->inode_head);
    m_super_block->bitmaps.main_driver = main_driver;
    m_super_block->bitmaps.sub_driver = sub_driver;
    m_super_block->bitmaps.block_no = 1;
    kassert(m_super_block->bitmaps.driver_type == m_super_block->driver_type);
    release_block_buffer(super_block_buffer);

    unlock_task();

    if(load_xfs_inode(main_driver, sub_driver, m_super_block->inode_head, &m_super_block->root_inode, timeout) < 0)
        return -1;

    node = super_block_hash_table[hash].next;
    lock_task();
    while(node != &super_block_hash_table[hash])
    {
        struct xfs_m_super_block_desc_t* super_block = parentof(node, struct xfs_m_super_block_desc_t, hash_node);
        if(super_block->driver_type == driver_type)
        {
            tslab_free(m_super_block, msp_size);
            break;
        }
        node = node->next;
    }

    unlock_task();
    return 0;
}

int format_using_xfs(uint16_t main_driver, uint16_t sub_driver, int timeout)
{
    struct block_driver_info_t driver_info;
    if(block_driver_get_driver_info(main_driver, sub_driver, &driver_info) < 0)
        return -1;
    if(driver_info.driver_block_count == 0 || driver_info.driver_block_size == 0)
        return -1;

    lock_task();
    long cur_jiffies = jiffies;
    struct block_buffer_desc_t* super_block_buffer = xfs_load_block(main_driver, sub_driver, 0, timeout);
    long used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(super_block_buffer == NULL)
    {
        unlock_task();
        return -1;
    }
    kassert(!(super_block_buffer->flags & BLKBUFFER_FLAG_UNSYNC));
    struct xfs_hd_super_block_desc_t* hd_super_block = (struct xfs_hd_super_block_desc_t*) super_block_buffer->buffer;

    hd_super_block->flags = XFS_MAGIC_NO;
    hd_super_block->block_num = driver_info.driver_block_count / (4096 / driver_info.driver_block_size);
    hd_super_block->block_size = 4096;

    size_t bitmap_block_count = (hd_super_block->block_num + 4096 * 8 - 1) / (4096 * 8);
    hd_super_block->inode_head = bitmap_block_count + 1;

    size_t block_num = hd_super_block->block_num;
    size_t root_inode_pos = hd_super_block->inode_head;

    super_block_buffer->flags |= BLKBUFFER_FLAG_DIRTY;
    release_block_buffer(super_block_buffer);

    for(size_t i = 0; i < block_num / (4096 * 8); ++i)
    {
        cur_jiffies = jiffies;
        struct block_buffer_desc_t* bitmap = get_block_buffer(main_driver, sub_driver, i + 1, timeout);
        used_jiffies = jiffies - cur_jiffies;
        if(timeout > 0)
            timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
        if(bitmap == NULL)
        {
            unlock_task();
            return -1;
        }
        if(bitmap->flags & BLKBUFFER_FLAG_SYNCING)
        {
            cur_jiffies = jiffies;
            if(block_buffer_wait_op_finished(bitmap, timeout) < 0)
            {
                release_block_buffer(bitmap);
                unlock_task();
                return -1;
            }
            used_jiffies = jiffies - cur_jiffies;
            if(timeout > 0)
                timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
        }
        kassert(!(bitmap->flags & BLKBUFFER_FLAG_SYNCING));
        _memset(bitmap->buffer, 0, 4096);
        if(root_inode_pos >= i * 4096 * 8)
        {
            uint8_t* bitmap_st = (uint8_t*) bitmap->buffer;
            for(size_t i = 0; i < root_inode_pos - i * 4096 * 8 + 1; ++i)
                bitmap_st[i >> 3] |= (0x01 << (i & 0x07));
        }
        bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
        release_block_buffer(bitmap);
    }

    if(block_num % (4096 * 8) != 0)
    {
        cur_jiffies = jiffies;
        struct block_buffer_desc_t* bitmap = get_block_buffer(main_driver, sub_driver, block_num / (4096 * 8) + 1, timeout);
        used_jiffies = jiffies - cur_jiffies;
        if(timeout > 0)
            timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
        if(bitmap == NULL)
        {
            unlock_task();
            return -1;
        }
        if(bitmap->flags & BLKBUFFER_FLAG_SYNCING)
        {
            cur_jiffies = jiffies;
            if(block_buffer_wait_op_finished(bitmap, timeout) < 0)
            {
                release_block_buffer(bitmap);
                unlock_task();
                return -1;
            }
            used_jiffies = jiffies - cur_jiffies;
            if(timeout > 0)
                timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
        }
        kassert(!(bitmap->flags & BLKBUFFER_FLAG_SYNCING));
        _memset(bitmap->buffer, 0xffffffff, 4096);

        uint8_t* bitmap_st = (uint8_t*) bitmap->buffer;

        for(size_t i = 0; i < block_num % (4096 * 8); ++i)
            bitmap_st[i >> 3] &= (~(0x01 << (i & 0x07)));

        bitmap->flags |= BLKBUFFER_FLAG_DIRTY;
        release_block_buffer(bitmap);
    }

    cur_jiffies = jiffies;
    struct block_buffer_desc_t* root_inode_buffer = get_block_buffer(main_driver, sub_driver, root_inode_pos, timeout);
    used_jiffies = jiffies - cur_jiffies;
    if(timeout > 0)
        timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    if(root_inode_buffer == NULL)
    {
        unlock_task();
        return -1;
    }
    if(root_inode_buffer->flags & BLKBUFFER_FLAG_SYNCING)
    {
        cur_jiffies = jiffies;
        if(block_buffer_wait_op_finished(root_inode_buffer, timeout) < 0)
        {
            release_block_buffer(root_inode_buffer);
            unlock_task();
            return -1;
        }
        used_jiffies = jiffies - cur_jiffies;
        if(timeout > 0)
            timeout = timeout > used_jiffies ? timeout - used_jiffies : -1;
    }
    kassert(!(root_inode_buffer->flags & BLKBUFFER_FLAG_SYNCING));

    int res = xfs_formatting_init_root_inode(root_inode_buffer);

    root_inode_buffer->flags |= BLKBUFFER_FLAG_DIRTY;
    release_block_buffer(root_inode_buffer);

    unlock_task();
    return res;
}
