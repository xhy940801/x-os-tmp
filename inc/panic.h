#pragma once

void panic(const char* str);

void _static_assert_func(const char* name, const char* file, unsigned int line, const char* func);

#define kassert(cond) ((cond) ? (void) 0 : _static_assert_func(#cond, __FILE__, __LINE__, __func__) )
