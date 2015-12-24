#pragma once

enum _rb_tree_color { _RB_TREE_RED, _RB_TREE_BLACK };

struct rb_tree_node_t
{
    enum _rb_tree_color color;
    struct rb_tree_node_t* parent;
    struct rb_tree_node_t* left;
    struct rb_tree_node_t* right;
};

struct rb_tree_head_t
{
    struct rb_tree_node_t pre_root;
};

extern struct rb_tree_node_t nil[1];

void rb_tree_init(struct rb_tree_head_t* head, struct rb_tree_node_t* root);
void rb_tree_remove(struct rb_tree_head_t* head, struct rb_tree_node_t* node);
void rb_tree_rebalance(struct rb_tree_node_t* root, struct rb_tree_node_t* node);

struct rb_tree_node_t* rb_tree_next(struct rb_tree_head_t* head, struct rb_tree_node_t* node);
struct rb_tree_node_t* rb_tree_prev(struct rb_tree_head_t* head, struct rb_tree_node_t* node);