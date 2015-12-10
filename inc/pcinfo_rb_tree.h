#pragma once

#include "rb_tree.h"

void pcinfo_rb_tree_insert(struct rb_tree_head_t* head, struct rb_tree_node_t* node);
struct rb_tree_node_t* pcinfo_rb_tree_find(struct rb_tree_head_t* head, int pid);
