#ifndef __CPU_H__
#define __CPU_H__

#define cpu_snapshot(buf) {    \
    uint32_t stack;                             \
    asm(" movl %%esp, %0 \n" : "=r"(stack));    \
    asm(" pusha \n");                           \
    asm("movl %0, %%edi \n" : : "r"(buf));      \
    asm("movl %0, %%esi \n" : : "r"(stack));    \
    asm("movl %0, %%ecx \n" : : "r"(100));      \
    asm("rep movsb");                           \
    asm(" movl %0, %%esp \n" : : "r"(stack));   \
}

#endif // __CPU_H__
