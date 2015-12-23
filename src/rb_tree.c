#include "rb_tree.h"

#include "stddef.h"

struct rb_tree_node_t nil[1];

static inline void left_rotate(struct rb_tree_node_t* node)
{
    struct rb_tree_node_t* tmp = node->right;
    node->right = tmp->left;
    tmp->left->parent = node;

    if (node->parent->left == node)
        node->parent->left = tmp;
    else
        node->parent->right = tmp;
    tmp->parent = node->parent;
    node->parent = tmp;
    tmp->left = node;
}

static inline void right_rotate(struct rb_tree_node_t* node)
{
    struct rb_tree_node_t* tmp = node->left;
    node->left = tmp->right;
    tmp->right->parent = node;

    if (node->parent->left == node)
        node->parent->left = tmp;
    else
        node->parent->right = tmp;
    tmp->parent = node->parent;
    node->parent = tmp;
    tmp->right = node;
}

static inline struct rb_tree_node_t* get_min(struct rb_tree_node_t* node)
{
    while (node->left != nil)
        node = node->left;
    return node;
}

static inline void swap_node(struct rb_tree_node_t* a, struct rb_tree_node_t* b)
{
    struct rb_tree_node_t tmp;
    if (a->parent->left == a)
        a->parent->left = b;
    else
        a->parent->right = b;
    if (b->parent->left == b)
        b->parent->left = a;
    else
        b->parent->right = a;
    
    tmp.color = a->color;
    tmp.parent = a->parent;
    tmp.left = a->left;
    tmp.right = a->right;

    a->color = b->color;
    a->parent = b->parent;
    a->left = b->left;
    a->right = b->right;

    b->color = tmp.color;
    b->parent = tmp.parent;
    b->left = tmp.left;
    b->right = tmp.right;

    a->left->parent = a;
    a->right->parent = a;
    b->left->parent = b;
    b->right->parent = b;
}

static inline void rb_rmfix(struct rb_tree_node_t* head, struct rb_tree_node_t* node)
{
    while (1)
    {
        if (node == head || node->color == _RB_TREE_RED)
        {
            node->color = _RB_TREE_BLACK;
            break;
        }
        struct rb_tree_node_t* parent = node->parent;
        if (node == parent->left)
        {
            struct rb_tree_node_t* brother = parent->right;
            if (brother->color == _RB_TREE_RED)
            {
                brother->color = _RB_TREE_BLACK;
                parent->color = _RB_TREE_RED;
                left_rotate(parent);
                brother = parent->right;
            }
            if (brother->left->color == _RB_TREE_BLACK && brother->right->color == _RB_TREE_BLACK)
            {
                brother->color = _RB_TREE_RED;
                node = parent;
                continue;
            }
            if (brother->left->color == _RB_TREE_RED && brother->right->color == _RB_TREE_BLACK)
            {
                brother->color = _RB_TREE_RED;
                brother->left->color = _RB_TREE_BLACK;
                right_rotate(brother);
                brother = parent->right;
            }
            left_rotate(parent);
            brother->color = parent->color;
            parent->color = _RB_TREE_BLACK;
            brother->right->color = _RB_TREE_BLACK;
            break;
        }
        else
        {
            struct rb_tree_node_t* brother = parent->left;
            if (brother->color == _RB_TREE_RED)
            {
                brother->color = _RB_TREE_BLACK;
                parent->color = _RB_TREE_RED;
                right_rotate(parent);
                brother = parent->left;
            }
            if (brother->left->color == _RB_TREE_BLACK && brother->right->color == _RB_TREE_BLACK)
            {
                brother->color = _RB_TREE_RED;
                node = parent;
                continue;
            }
            if (brother->right->color == _RB_TREE_RED && brother->left->color == _RB_TREE_BLACK)
            {
                brother->color = _RB_TREE_RED;
                brother->right->color = _RB_TREE_BLACK;
                left_rotate(brother);
                brother = parent->left;
            }
            right_rotate(parent);
            brother->color = parent->color;
            parent->color = _RB_TREE_BLACK;
            brother->left->color = _RB_TREE_BLACK;
            break;
        }
    }
}

void rb_tree_init(struct rb_tree_head_t* head, struct rb_tree_node_t* root)
{
    head->pre_root.color = _RB_TREE_BLACK;
    head->pre_root.parent = root;
    head->pre_root.left = root;
    head->pre_root.right = root;
    root->parent = &(head->pre_root);
    root->color = _RB_TREE_BLACK;
    root->left = nil;
    root->right = nil;
}

void rb_tree_remove(struct rb_tree_head_t* head, struct rb_tree_node_t* node)
{
    if (node->left != nil && node->right != nil)
    {
        struct rb_tree_node_t* min = get_min(node->right);
        if (min != node->right)
            swap_node(node, min);
        else
        {
            node->right = min->right;
            min->right->parent = node;

            min->left = node->left;
            node->left->parent = min;

            min->parent = node->parent;
            if (node->parent->left == node)
                node->parent->left = min;
            else
                node->parent->right = min;

            min->right = node;
            node->parent = min;

            enum _rb_tree_color ctmp = node->color;
            node->color = min->color;
            min->color = ctmp;
            node->left = nil;
        }
    }
    enum _rb_tree_color onodec;
    if (node->right == nil)
    {
        node->left->parent = node->parent;
        if (node->parent->left == node)
            node->parent->left = node->left;
        else
            node->parent->right = node->left;
        onodec = node->color;
        node = node->left;
    }
    else if(node->left == nil)
    {
        node->right->parent = node->parent;
        if (node->parent->left == node)
            node->parent->left = node->right;
        else
            node->parent->right = node->right;
        onodec = node->color;
        node = node->right;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"
    if(onodec == _RB_TREE_BLACK)
        rb_rmfix(head->pre_root.left, node);
#pragma GCC diagnostic pop
}

void rb_tree_rebalance(struct rb_tree_node_t* root, struct rb_tree_node_t* node)
{
    node->color = _RB_TREE_RED;
    while (1)
    {
        if (node == root)
        {
            node->color = _RB_TREE_BLACK;
            break;
        }
        struct rb_tree_node_t* parent = node->parent;
        struct rb_tree_node_t*  grandparent = parent->parent;
        if (parent->color == _RB_TREE_BLACK)
            break;
        if (grandparent->left == parent)
        {
            if (grandparent->right->color == _RB_TREE_RED)
            {
                grandparent->right->color = _RB_TREE_BLACK;
                parent->color = _RB_TREE_BLACK;
                grandparent->color = _RB_TREE_RED;
                node = grandparent;
                continue;
            }
            if (parent->right == node)
            {
                left_rotate(parent);
                node = parent;
                parent = node->parent;
            }
            right_rotate(grandparent);
            parent->color = _RB_TREE_BLACK;
            grandparent->color = _RB_TREE_RED;
            break;
        }
        else
        {
            if (grandparent->left->color == _RB_TREE_RED)
            {
                grandparent->left->color = _RB_TREE_BLACK;
                parent->color = _RB_TREE_BLACK;
                grandparent->color = _RB_TREE_RED;
                node = grandparent;
                continue;
            }
            if (parent->left == node)
            {
                right_rotate(parent);
                node = parent;
                parent = node->parent;
            }
            left_rotate(grandparent);
            parent->color = _RB_TREE_BLACK;
            grandparent->color = _RB_TREE_RED;
            break;
        }
    }
}

struct rb_tree_node_t* rb_tree_next(struct rb_tree_head_t* head, struct rb_tree_node_t* node)
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

struct rb_tree_node_t* rb_tree_prev(struct rb_tree_head_t* head, struct rb_tree_node_t* node)
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


