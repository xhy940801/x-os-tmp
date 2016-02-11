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
    union
    {
        struct xfs_hd_inode_clause_desc_t clauses[0];
        char data[0];
    };
};

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
#if (SUPER_BLOCK_HASH_TABLE_SIZE & (SUPER_BLOCK_HASH_TABLE_SIZE - 1))
    return driver_type % SUPER_BLOCK_HASH_TABLE_SIZE;
#else
    return driver_type & (SUPER_BLOCK_HASH_TABLE_SIZE - 1);
#endif
}

void init_xfs_module()
{
    for(size_t i = 0; i < SUPER_BLOCK_HASH_TABLE_SIZE; ++i)
        circular_list_init(&super_block_hash_table[i]);
    printk("xfs_m_inode size [%u]\n", sizeof(struct xfs_m_inode_desc_t));
}

    return buffer;
static inline struct block_buffer_desc_t* xfs_block_buffer_weak_pointer_get_without_unsync(struct block_buffer_weak_pointer_desc_t* weak_pointer, int timeout)
{
    struct block_buffer_desc_t* buffer = block_buffer_weak_pointer_get(&inode->main_inode, timeout);
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

static inline struct block_buffer_desc_t* xfs_block_buffer_without_unsync(uint16_t main_driver, uint16_t sub_driver, uint32_t block_no, int timeout)
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


static inline ssize_t xfs_inode_read_level0(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len)
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
    uint32_t pos = inode->inode_base.pos;
    uint32_t size = inode->size;
    len = pos + len > size ? size - len : len;
    len = pos + len > 4096 - sizeof(struct xfs_hd_inode_desc_t) ? 4096 - sizeof(struct xfs_hd_inode_desc_t) : len;
    kassert(len <= 4096);
    if(len == 0)
    {
        release_block_buffer(main_inode_buffer);
        return 0;
    }
    _memcpy(buf, &hd_inode->data[pos], len);
    inode->inode_base.pos += len;
    release_block_buffer(main_inode_buffer);
    return (ssize_t) len;
}

static inline ssize_t xfs_inode_read_level1(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len)
{
    uint32_t pos = inode->inode_base.pos;
    if((pos & (~(4096 - 1))) != inode->last_used_block_start)
    {
        struct block_buffer_desc_t* main_inode_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->main_inode, 0);
        if(main_inode_buffer == NULL)
            return -1;
        inode_level = (inode->inode_base.flags << 24) & 0x0f;
        if(inode_level != 1)
        {
            release_block_buffer(main_inode_buffer);
            return -2;
        }
        pos = inode->inode_base.pos;
        if(pos >= inode->size)
        {
            release_block_buffer(main_inode_buffer);
            return 0;
        }
        struct xfs_hd_inode_desc_t* hd_inode = (struct xfs_hd_inode_desc_t*) main_inode_buffer->buffer;
        size_t inode_pos = 0;
        for(size_t i = 0; ; ++i)
        {
            if(hd_inode->clauses[i].inode_count == 0 || pos < inode_pos + 4096 * hd_inode->clauses[i].inode_count)
                break;
            inode_pos += 4096 * hd_inode->clauses[i].inode_count;
        }
        int32_t target_lba = hd_inode->clauses[i].start_offset;
        target_lba += ((int32_t) main_inode_buffer->block_no) * 4096;
        target_lba += (int32_t) (pos - inode_pos);
        inode->last_used_block_start = pos & (~(4096 - 1));
        inode->last_used_block.block_no = target_lba / 4096;
        release_block_buffer(main_inode_buffer);
    }
    struct block_buffer_desc_t* target_block_buffer = xfs_block_buffer_weak_pointer_get_without_unsync(&inode->last_used_block, 0);
    pos = inode->inode_base.pos;
    size_t size = inode->size;
    if(pos >= size)
    {
        release_block_buffer(target_block_buffer);
        return 0;
    }
    if((pos & (~(4096 - 1))) != inode->last_used_block_start)
    {
        release_block_buffer(target_block_buffer);
        return -2;
    }
    len = pos + len > size ? size - len : len;
    len = (pos + len) /4096 != pos / 4096 ? ((pos + len) & (~(4096 - 1))) - pos : len;
    kassert(len <= 4096 && len != 0);
    char* data = target_block_buffer->buffer;
    _memcpy(buf, &data[pos & (4096 - 1)], len);
    release_block_buffer(target_block_buffer);
    return (ssize_t) len;
}

static inline ssize_t xfs_inode_read_level_other(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len)
{

}

static inline ssize_t xfs_inode_read_once(volatile struct xfs_m_inode_desc_t* inode, char* buf, size_t len)
{
    size_t inode_level = (inode->inode_base.flags << 24) & 0x0f;
    if(inode_level == 0)
        return xfs_inode_read_level0(inode, buf, len);
    else if(inode_level == 1)
        return xfs_inode_read_level1(inode, buf, len);
    else
        return xfs_inode_read_level_other(inode, buf, len);
}

ssize_t xfs_inode_read(struct vfs_inode_desc_t* _inode, char* buf, size_t len)
{
    volatile struct xfs_m_inode_desc_t* inode = parentof(_inode, struct xfs_m_inode_desc_t, inode_base);
    kassert(inode->inode_base.fsys_type == VFS_TYPE_XFS);
    size_t has_read = 0;
    while(has_read < len)
    {
        lock_task();
        ssize_t res = xfs_inode_read_once(inode, buf + has_read, len - has_read);
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

struct vfs_inode_desc_t* xfs_open_root_inode(uint16_t main_driver, uint16_t sub_driver, int timeout)
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
    m_inode->inode_base.pos = 0;
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