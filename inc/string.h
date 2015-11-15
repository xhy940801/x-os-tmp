#pragma once

#ifndef TESTMODEL

#include "stddef.h"

void _memcpy(void* dst, void* src, size_t size);
void _memset(void* dst, int value, size_t size);
void _memmove(void* dst, void* src, size_t size);

size_t strlen(const char* str);

#else

#include <string.h>

#define _memcpy memcpy
#define _memset memset
#define _memmove memmove

#endif
