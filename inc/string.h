#pragma once

#include "stddef.h"

void _memcpy(void* dst, void* src, size_t size);
void _memset(void* dst, int value, size_t size);
void _memmove(void* dst, void* src, size_t size);
