#include "pcinfo_rb_tree.h"

#include "stddef.h"

#include "sched.h"

void pcinfo_rb_tree_insert(struct rb_tree_head_t* head, struct rb_tree_node_t* node)
{
    node->left = nil;
    node->right = nil;
    struct rb_tree_node_t* p = head->pre_root.left;
    while (1)
    {
        if (parentof(node, struct process_info_t, rb_node)->pid < parentof(p, struct process_info_t, rb_node)->pid)
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

struct rb_tree_node_t* pcinfo_rb_tree_find(struct rb_tree_head_t* head, int pid)
{
    struct rb_tree_node_t* p = head->pre_root.left;
    while(1)
    {
        if (pid == parentof(p, struct process_info_t, rb_node)->pid)
            return p;
        if(pid < parentof(p, struct process_info_t, rb_node)->pid)
        {
            if (p->left == nil)
                return NULL;
            p = p->left;
        }
        else
        {
            if (p->right == nil)
                return NULL;
            p = p->right;
        }

    }
}
