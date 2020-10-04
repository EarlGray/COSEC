#ifndef __COSEC_LIBC_COSEC_SYS__
#define __COSEC_LIBC_COSEC_SYS__

#include <cosec/syscall.h>

static inline
int syscall(int num, intptr_t arg1, intptr_t arg2, intptr_t arg3) {
    int ret;
    asm(
        "movl %3, %%ebx         \n"
        "movl %2, %%edx         \n"
        "movl %1, %%ecx         \n"
        "movl %0, %%eax         \n"
        "int $0x80              \n"
        :"=a"(ret):"m"(num), "m"(arg1), "m"(arg2), "m"(arg3)
    );
    return ret;
}

#define __syscall0(num)             syscall((num), 0, 0, 0)
#define __syscall1(num, a)          syscall((num), (a), 0, 0)
#define __syscall2(num, a, b)       syscall((num), (a), (b), 0)
#define __syscall3(num, a, b, c)    syscall((num), (a), (b), (c))


#endif  // __COSEC_LIBC_COSEC_SYS__
