#pragma once

/**
 * heap[0] is heap size
 * 2            1           10
 * |            |
 * heap size    heap root
 */

void min_heap_init(int* heap);
void min_heap_push(int* heap, int val);
void min_heap_pop(int* heap);
