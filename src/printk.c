#include "asm.h"
#include "string.h"
#include "stdint.h"
#include "stddef.h"
#include "limits.h"

#include "vfs.h"

void _printchar(char c)
{
    sys_write(1, &c, 1);
}

void _printstr(const char* str)
{
    sys_write(1, str, strlen(str));
}

void _printint(int num)
{
    if(num == INT_MIN)
    {
        _printstr("-2147483648");
        return;
    }
    if(num < 0)
    {
        _printchar('-');
        num = 0 - num;
    }
    char tmp[16];
    size_t i = 0;
    do
    {
        tmp[i] = (num % 10) + '0';
        num /= 10;
        ++i;
    } while(num);
    --i;
    while(i < 16)
    {
        _printchar(tmp[i]);
        --i;
    }
}

static void _printuint(unsigned int num)
{
    char tmp[16];
    size_t i = 0;
    do
    {
        tmp[i] = (num % 10) + '0';
        num /= 10;
        ++i;
    } while(num);
    --i;
    while(i < 16)
    {
        _printchar(tmp[i]);
        --i;
    }

}

void printk(const char* fmt, ...)
{
    void* params = ((char*) &fmt) + 4;
    while(*fmt)
    {
        if(*fmt == '%')
        {
            if(fmt[1] == 'd')
                _printint(*((int*) params));
            else if(fmt[1] == 'u')
                _printuint(*((unsigned int*) params));
            else if(fmt[1] == 'c')
                _printchar(*((char*) params));
            else if(fmt[1] == 's')
                _printstr(*((char**) params));
            else if(fmt[1] == '\0')
                break;
            params = ((char*) params) + 4;
            ++fmt;
        }
        else
            _printchar(*fmt);
        ++fmt;
    }
}
