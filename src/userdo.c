static int fork()
{
    int _v;
    __asm__ (
        "mov $1, %%eax\n"
        "int $0x80\n"
        "mov %%eax, %0"
        :"=g"(_v)
        :
        :"eax","ebx","ecx","edx","memory"
    );
    return _v;
}

static void yield()
{
    __asm__ (
        "mov $2, %%eax\n"
        "int $0x80\n"
        :::"eax","ebx","ecx","edx","memory"
    );

}

static void tsleep(int n)
{
    __asm__ (
        "mov $3, %%eax\n"
        "mov %0, %%ecx\n"
        "int $0x80\n"
        ::"g"(n)
        :"eax","ebx","ecx","edx","memory"
    );
}

void user_do()
{
    volatile int ret;
    ret = fork();
    if(ret == 0)
    {
        tsleep(10000);
        while(1)
            __asm__ volatile("nop;");
    }
    else
    {
        while(1);
    }
    while(1);
}


