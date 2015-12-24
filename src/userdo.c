#include "unistd.h"

#include "string.h"

#pragma GCC push_options
#pragma GCC optimize ("O0")

void putstr(const char* str)
{
    write(1, str, strlen(str));
}

void user_do()
{
    char str1[] = "hehe\n";
    //char str2[] = "subAAAAAA\n";
    //char str3[] = "mai22333333333333333333333\n";
    volatile int ret;
    putstr(str1);
    ret = fork();
    if(ret == 0)
    {
        syscall0(0x50);
    }
    else
    {
        while(1);
    }
    while(1);
}

#pragma GCC pop_options
