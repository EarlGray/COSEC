#define NOT_CC

#include <mem/paging.h>
#include <arch/setjmp.h>

.text

#define SIZEOFINT 4

.global i386_setjmp
.global i386_longjmp

/*
 * int setjmp(jmp_buf env)
 */
i386_setjmp:
    movl 4(%esp), %edx         # &jmp_buf
    movl (%esp),  %ecx         # ret addr

    # save registers protected by cdecl
    movl %ecx, (SIZEOFINT * JMPBUF_EIP_OFFSET)(%edx)
    movl %esp, (SIZEOFINT * JMPBUF_ESP_OFFSET)(%edx)
    movl %ebx, (SIZEOFINT * JMPBUF_EBX_OFFSET)(%edx)
    movl %esi, (SIZEOFINT * JMPBUF_ESI_OFFSET)(%edx)
    movl %edi, (SIZEOFINT * JMPBUF_EDI_OFFSET)(%edx)
    movl %ebp, (SIZEOFINT * JMPBUF_EBP_OFFSET)(%edx)

    xor %eax, %eax
    ret

/*
 * int longjmp(jmp_buf env, val)
 */
i386_longjmp:
    movl 4(%esp), %edx          # &jmp_buf
    movl 8(%esp), %eax          # val

    # restore saved regisers
    movl (SIZEOFINT * JMPBUF_EBP_OFFSET)(%edx), %ebp
    movl (SIZEOFINT * JMPBUF_EDI_OFFSET)(%edx), %edi
    movl (SIZEOFINT * JMPBUF_ESI_OFFSET)(%edx), %esi
    movl (SIZEOFINT * JMPBUF_EBX_OFFSET)(%edx), %ebx
    movl (SIZEOFINT * JMPBUF_ESP_OFFSET)(%edx), %esp
    movl (SIZEOFINT * JMPBUF_EIP_OFFSET)(%edx), %ecx  # return address

    movl %edx, 4(%esp)
    movl %ecx, (%esp)

    movl $1, %ecx
    test %eax, %eax
    cmovzl %ecx, %eax  # set return value to 1 if received val==0

    ret

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
    movw $043, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    addl $4, %esp /* discard %eip */
    iret

.global i386_switch_pagedir
i386_switch_pagedir:
    movl 4(%esp), %eax

    movl %cr0, %edx
    andl $(~CR0_PG), %edx
    movl %edx, %cr0     // disable paging 

    movl %eax, %cr3

    movl %cr0, %eax
    orl  $(CR0_PG | CR0_WP), %eax
    movl %eax, %cr0         // enable paging and write-protection

    ret

