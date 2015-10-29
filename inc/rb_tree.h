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
