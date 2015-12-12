#pragma once

#include "rb_tree.h"

#include "stdint.h"
#include "stddef.h"

struct rb_tree_node_t* blkbuffer_rb_tree_find(struct rb_tree_head_t* head, uint16_t main_driver, uint16_t sub_driver, size_t block_no);
struct rb_tree_node_t* blkbuffer_rb_tree_next(struct rb_tree_head_t* head, struct rb_tree_node_t* node);
struct rb_tree_node_t* blkbuffer_rb_tree_prev(struct rb_tree_head_t* head, struct rb_tree_node_t* node);
void blkbuffer_rb_tree_insert(struct rb_tree_head_t* head, struct rb_tree_node_t* node);
