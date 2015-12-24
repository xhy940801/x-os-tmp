#include "blkbuffer_rb_tree.h"

#include "stddef.h"

#include "block_buffer.h"

static inline int blk_less(uint32_t driver_type1, size_t block_no1, uint32_t driver_type2, size_t block_no2)
{
    return driver_type1 == driver_type2 ? block_no1 < block_no2 : driver_type1 < driver_type2;
}

static inline int blk_less2(struct block_buffer_desc_t* blk1, struct block_buffer_desc_t* blk2)
{
    return blk_less(blk1->driver_type, blk1->block_no, blk2->driver_type, blk2->block_no);
}

struct rb_tree_node_t* blkbuffer_rb_tree_find(struct rb_tree_head_t* head, uint16_t main_driver, uint16_t sub_driver, size_t block_no)
{
    uint32_t driver_type = sub_driver | main_driver << 16;
    struct rb_tree_node_t* p = head->pre_root.left;
    while(p != nil)
    {
        struct block_buffer_desc_t* blk = parentof(p, struct block_buffer_desc_t, rb_tree_node);
        if(blk_less(driver_type, block_no, blk->driver_type, blk->block_no))
            p = p->left;
        else if(blk_less(blk->driver_type, blk->block_no, driver_type, block_no))
            p = p->right;
        else
            return p;
    }
    return NULL;
}

void blkbuffer_rb_tree_insert(struct rb_tree_head_t* head, struct rb_tree_node_t* node)
{
    node->left = nil;
    node->right = nil;
    struct rb_tree_node_t* p = head->pre_root.left;
    while (1)
    {
        if (
            blk_less2(
                parentof(node, struct block_buffer_desc_t, rb_tree_node),
                parentof(p, struct block_buffer_desc_t, rb_tree_node))
            )
        {
            if (p->left == nil)
            {
                p->left = node;
                break;
            }
            p = p->left;
        }
        else
        {
            if (p->right == nil)
            {
                p->right = node;
                break;
            }
            p = p->right;
        }
    }
    node->parent = p;
    rb_tree_rebalance(head->pre_root.left, node);
}
