#pragma once

struct list_node_t
{
    struct list_node_t* next;
    struct list_node_t* pre;
};

void circular_list_init(struct list_node_t* head);
void circular_list_insert(struct list_node_t* position, struct list_node_t* node);
void circular_list_remove(struct list_node_t* node);
