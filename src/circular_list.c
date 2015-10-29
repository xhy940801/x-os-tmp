#include "circular_list.h"

void circular_list_init(struct list_node_t* head)
{
    head->next = head;
    head->pre = head;
}

void circular_list_insert(struct list_node_t* position, struct list_node_t* node)
{
    node->next = position->next;
    node->pre = position;

    position->next = node;
    node->next->pre = node;
}

void circular_list_remove(struct list_node_t* node)
{
    node->next->pre = node->pre;
    node->pre->next = node->next;
}
