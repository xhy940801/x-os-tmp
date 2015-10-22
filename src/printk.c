#include "asm.h"
#include "string.h"
#include "stdint.h"
#include "stddef.h"
#include "limits.h"

#define VGA_MEM_START_POS (0xfffb8000)

static uint32_t cursor_pos = 0;

void _printchar(char c, char type)
{
    if(c == '\n')
    {
        cursor_pos = (cursor_pos / 80 + 1) * 80;
    }
    else if(c == '\b')
    {
        char* pos = (char*) (VGA_MEM_START_POS + (cursor_pos << 1));
        pos[0] = 0;
        pos[1] = 0;
        cursor_pos = cursor_pos > 0 ? cursor_pos - 1 : 0;
    }
    else if(c == '\r')
        cursor_pos = cursor_pos / 80 * 80;
    else if(c == '\0')
    {}
    else if(c == '\t')
        cursor_pos = (cursor_pos & (~ 0x07)) + 8;
    else if(c == '\v')
        cursor_pos += 80;
    else if(c == '\f')
    {
        _memset((void*) VGA_MEM_START_POS, 0x07000700, 4000);
        cursor_pos = 0;
    }
    else
    {
        char* pos = (char*) (VGA_MEM_START_POS + (cursor_pos << 1));
        pos[0] = c;
        pos[1] = type;
        ++cursor_pos;
    }
    while(cursor_pos >= 2000)
    {
        _memmove((void*) VGA_MEM_START_POS, (void*) (VGA_MEM_START_POS + 160), 3840);
        _memset((void*) VGA_MEM_START_POS + 3840, 0x07000700, 160);
        cursor_pos -= 80;
    }
}

void _flush_cursor(char type)
{
    char* pos = (char*) (VGA_MEM_START_POS + (cursor_pos << 1));
    pos[1] = type;
    _outb(0x3d4, 0x0f);
    _outb(0x3d5, cursor_pos & 0xff);
    _outb(0x3d4, 0x0e);
    _outb(0x3d5, (cursor_pos >> 8) & 0xff);
}

void _printstr(const char* str, char type)
{
    while(*str)
    {
        _printchar(*str, type);
        ++str;
    }
}

void _printint(int num, char type)
{
    if(num == INT_MIN)
    {
        _printstr("-2147483648", type);
        return;
    }
    if(num < 0)
    {
        _printchar('-', type);
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
        _printchar(tmp[i], type);
        --i;
    }
}

void _printuint(unsigned int num, char type)
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
        _printchar(tmp[i], type);
        --i;
    }

}

void printk(const char* fmt, ...)
{
    static char type = 0x07;
    void* params = ((char*) &fmt) + 4;
    while(*fmt)
    {
        if(*fmt == 27)
        {
            type = fmt[1];
            ++fmt;
            if(*fmt == '\0')
                break;
        }
        else if(*fmt == '%')
        {
            if(fmt[1] == 'd')
                _printint(*((int*) params), type);
            else if(fmt[1] == 'u')
                _printuint(*((unsigned int*) params), type);
            else if(fmt[1] == 'c')
                _printchar(*((char*) params), type);
            else if(fmt[1] == 's')
                _printstr(*((char**) params), type);
            else if(fmt[1] == '\0')
                break;
            params = ((char*) params) + 4;
            ++fmt;
        }
        else
            _printchar(*fmt, type);
        ++fmt;
    }
    _flush_cursor(type);
}
