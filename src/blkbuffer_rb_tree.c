#include "blkbuffer_rb_tree.h"

#include "stddef.h"

#include "block_buffer.h"

static inline int blk_less(uint32_t driver_type1, size_t block_no1, uint32_t driver_type2, size_t block_no2)
{
    return driver_type1 == driver_type2 ? block_no1 < block_no2 : driver_type1 < driver_type2;
}

struct rb_tree_node_t* blkbuffer_rb_tree_find(struct rb_tree_head_t* head, uint16_t main_driver, uint16_t sub_driver, size_t block_no)
{
    uint32_t driver_type = sub_driver | main_driver >> 16;
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


struct rb_tree_node_t* blkbuffer_rb_tree_next(struct rb_Tree_head_t* head, struct rb_tree_node_t* node)
{
    if(node->right != nil)
    {
        node = node->right;
        while(node->left != nil)
            node = node->left;
        return node;
    }
    while(node != head->pre_root.left)
    {
        if(node->parent->left == node)
            return node->parent;
        node = node->parent;
    }
    return NULL;
}


struct rb_tree_node_t* blkbuffer_rb_tree_prev(struct rb_tree_head_t* head, struct rb_tree_node_t* node)
{
    if(node->left != nil)
    {
        node = node->left;
        while(node->right != nil)
            node = node->right;
        return node;
    }
    while(node != head->pre_root.left)
    {
        if(node->parent->right == node)
            return node->parent;
        node = node->parent;
    }
    return NULL;
}
