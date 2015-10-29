#pragma once

#include "rb_tree.h"

void pcinfo_rb_tree_init(struct rb_tree_head_t* head, struct rb_tree_node_t* root);
void pcinfo_rb_tree_insert(struct rb_tree_head_t* head, struct rb_tree_node_t* node);
void pcinfo_rb_tree_remove(struct rb_tree_head_t* head, struct rb_tree_node_t* node);
