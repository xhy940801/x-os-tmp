#include "unistd.h"


int fork()
{
    return syscall0(0x01);
}

void sched_yield()
{
    syscall0(0x02);
}

void tsleep(uint32_t n)
{
    syscall1(0x03, n);
}

void write(int fd, const char* buf, size_t len)
{
    syscall3(0x10, fd, buf, len);
}

void read(int fd, char* buf, size_t len)
{
    syscall3(0x11, fd, buf, len);
}
