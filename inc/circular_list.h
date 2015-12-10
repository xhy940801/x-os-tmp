#pragma once

#include "stddef.h"

struct list_node_t
{
    struct list_node_t* next;
    struct list_node_t* pre;
};

void circular_list_init(struct list_node_t* head);
void circular_list_insert(struct list_node_t* position, struct list_node_t* node);
void circular_list_remove(struct list_node_t* node);
int circular_list_is_inlist(struct list_node_t* head, struct list_node_t* node);
size_t circular_list_size(struct list_node_t* head);

inline int circular_list_is_empty(struct list_node_t* head)
{
    return head->next == head;
}
