#include "unistd.h"

void user_do()
{
    volatile int ret;
    ret = fork();
    if(ret == 0)
    {
        ret = fork();
        while(1);
    }
    else
    {
        while(1);
    }
    while(1);
}

