#include "tty0.h"

#include "stddef.h"
#include "string.h"

#include "asm.h"
#include "vfs.h"
#include "task_locker.h"

struct tty0_vfs_inode_desc_t
{
    struct vfs_inode_desc_t inode_base;
    uint32_t pos;
    char color;
    uint32_t state;
    uint32_t tty_base_addr;
};

static struct tty0_vfs_inode_desc_t tty0_inode;
static struct vfs_desc_t tty0_vfs;

static void _flush_cursor(struct tty0_vfs_inode_desc_t* inode)
{
    char* pos = (char*) (inode->tty_base_addr + (inode->pos << 1));
    pos[1] = inode->color;
    _outb(0x3d4, 0x0f);
    _outb(0x3d5, inode->pos & 0xff);
    _outb(0x3d4, 0x0e);
    _outb(0x3d5, (inode->pos >> 8) & 0xff);
}

static int tty0_write(struct vfs_inode_desc_t* _inode, const char* buf, size_t len, struct fd_struct_t* fd_struct)
{
    struct tty0_vfs_inode_desc_t* inode = parentof(_inode, struct tty0_vfs_inode_desc_t, inode_base);
    size_t i;
    for(i = 0; i < len; ++i)
    {
        char c = buf[i];
        if((i & 0x1f) == 0x1f)
        {
            lock_task();
            unlock_task();
        }
        if(inode->state == TTY0_STATE_NORMAL)
        {
            if(c == 27)
            {
                inode->state = TTY0_STATE_SETTING;
                continue;
            }
            if(c == '\n')
            {
                inode->pos = (inode->pos / 80 + 1) * 80;
            }
            else if(c == '\b')
            {
                char* pos = (char*) (inode->tty_base_addr + (inode->pos << 1));
                pos[0] = 0;
                pos[1] = 0;
                inode->pos = inode->pos > 0 ? inode->pos - 1 : 0;
            }
            else if(c == '\r')
                inode->pos = inode->pos / 80 * 80;
            else if(c == '\0')
            {}
            else if(c == '\t')
                inode->pos = (inode->pos & (~ 0x07)) + 8;
            else if(c == '\v')
                inode->pos += 80;
            else if(c == '\f')
            {
                _memset((void*) inode->tty_base_addr, 0x07000700, 4000);
                inode->pos = 0;
            }
            else
            {
                char* pos = (char*) (inode->tty_base_addr + (inode->pos << 1));
                pos[0] = c;
                pos[1] = inode->color;
                ++inode->pos;
            }
            while(inode->pos >= 2000)
            {
                _memmove((void*) inode->tty_base_addr, (void*) (inode->tty_base_addr + 160), 3840);
                _memset((void*) inode->tty_base_addr + 3840, 0x07000700, 160);
                inode->pos -= 80;
            }
        }
        else
        {
            inode->color = c;
            inode->state = TTY0_STATE_NORMAL;
        }
    }
    _flush_cursor(inode);
    return i;
}

static struct vfs_inode_desc_t* get_tty0_inode(uint16_t main_driver, uint16_t sub_driver)
{
    return &tty0_inode.inode_base;
}

void init_tty0_module()
{
    _memset(&tty0_vfs, 0, sizeof(tty0_vfs));
    tty0_vfs.name = "tty0";
    tty0_vfs.write = tty0_write;
    tty0_vfs.get_root_inode = get_tty0_inode;

    tty0_inode.inode_base.fsys_type = VFS_TYPE_TTY0;
    tty0_inode.inode_base.main_driver = VFS_MDRIVER_TTY0;
    tty0_inode.inode_base.sub_driver = 1;
    tty0_inode.inode_base.flags = VFS_MOD_UWRITE | VFS_MOD_GWRITE | VFS_MOD_OWRITE;
    tty0_inode.color = 0x07;
    tty0_inode.state = TTY0_STATE_NORMAL;
    tty0_inode.tty_base_addr = 0xfffb8000;

    vfs_register(&tty0_vfs, VFS_TYPE_TTY0);
}