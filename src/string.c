#include "string.h"

#include "stddef.h"

size_t strlen(const char* str)
{
    size_t i;
    for(i = 0; str[i] != '\0'; ++i);
    return i;
}
