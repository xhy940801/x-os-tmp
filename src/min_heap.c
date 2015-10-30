#include "min_heap.h"

#include "stddef.h"
#include "limits.h"

void min_heap_init(int* heap)
{
    size_t size = heap[0];
    size_t half = size >> 1;
    for(size_t i = half; i > 0; --i)
    {
        size_t j = i;
        int val = heap[j];
        do
        {
            size_t d = j << 1;
            if(val < heap[d] && val < heap[d + 1])
                break;
            if(heap[d] < heap[d + 1])
            {
                heap[j] = heap[d];
                j = d;
            }
            else
            {
                heap[j] = heap[d + 1];
                j = d + 1;
            }
        } while(j <= half);
        heap[j] = val;
    }
}

void min_heap_push(int* heap, int val)
{
    size_t size = heap[0];
    ++heap[0];
    size_t i;
    for(i = size + 1; i > 1;)
    {
        size_t half = i >> 1;
        if(val > heap[half])
            break;
        heap[i] = heap[half];
        i = half;
    }
    heap[i] = val;
}

void min_heap_pop(int* heap)
{
    size_t size = heap[0];
    size_t half = size >> 1;
    int val = heap[size];
    heap[size] = INT_MAX;
    size_t i;
    for(i = 1; i <= half;)
    {
        size_t d = i << 1;
        if(val < heap[d] && val < heap[d + 1])
            break;
        if(heap[d] < heap[d + 1])
        {
            heap[i] = heap[d];
            i = d;
        }
        else
        {
            heap[i] = heap[d + 1];
            i = d + 1;
        }
    }
    heap[i] = val;
    --heap[0];
}
