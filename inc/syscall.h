#pragma once

#define SYSCALL_SIZE 256

void init_syscall_module();

int syscall_register(int num, void* callback);
