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

int circular_list_is_inlist(struct list_node_t* head, struct list_node_t* node)
{
    for(struct list_node_t* p = head->next; p != head; p = p->next)
        if(p == node)
            return 1;
    return 0;
}

size_t circular_list_size(struct list_node_t* head)
{
    size_t len = 0;
    for(struct list_node_t* p = head->next; p != head; p = p->next)
        ++len;
    return len;
}