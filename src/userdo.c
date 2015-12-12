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
    while(1);
    char str1[] = "hehe\n";
    char str2[] = "sub\n";
    char str3[] = "main\n";
    volatile int ret;
    putstr(str1);
    ret = fork();
    if(ret == 0)
    {
        ret = fork();
        putstr(str2);
        while(1);
    }
    else
    {
        //sched_yield();
        putstr(str3);
        while(1);
    }
    while(1);
}

#pragma GCC pop_options
