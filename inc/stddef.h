#pragma once

#ifndef TESTMODEL

typedef unsigned int        size_t  ;
typedef int                 ssize_t ;

#undef NULL
#define NULL 0

#undef offsetof
#define offsetof(type, member) ((size_t) &(((type*) 0)->member))

#undef parentof
#define parentof(source, type, member) ((type*) (((char*) source) - offsetof(type, member)))

#else

#include <stddef.h>
#include <sys/types.h>
#undef parentof
#define parentof(source, type, member) ((type*) (((char*) source) - offsetof(type, member)))

#endif
