#define ASM

#include <globl.h>

.text

.global i386_snapshot
i386_snapshot:
    movl 4(%esp), %edi
    pusha
    movl %esp, %esi
    movl $8, %ecx
    rep movsl
    popa
    ret

.global i386_rdtsc
i386_rdtsc:
    movl 4(%esp), %edi
    rdtsc
    movl %eax, (%edi)
    movl %edx, 4(%edi)
    ret

.global start_userspace
start_userspace:
    addl $4, %esp
    iret
