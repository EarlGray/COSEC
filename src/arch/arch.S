#define NOT_CC

#include <globl.h>
#include <mem/paging.h>

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

.global _temp_delete_me_asm
_temp_delete_me_asm:
    movl %eax, (,%ebp)
    movl %eax, 0x42(,%ebp)
    movl %eax, (,%edx)
    movl %eax, 0x42(,%edx)
    movl %eax, (,%ebp,2)
    movl %eax, 0x42(,%ebp,2)
    movl %eax, (,%edx,2)
    movl %eax, 0x42(,%edx,2)
