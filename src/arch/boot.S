#define NOT_CC
/*
#define __ELF__  
*/

#include "arch/multiboot.h"
#include "mem/paging.h"

#define CR4_PSE     0x00000010
#define CR4_PGE     0x00000080

.text
jmp_start:
    jmp multiboot_entry

.align  4
multiboot_header:
    .long MULTIBOOT_HEADER_MAGIC
    .long MULTIBOOT_HEADER_FLAGS
    .long -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
#ifndef __ELF__
    .long multiboot_header
    .long _start
    .long _edata
    .long _end
    .long multiboot_entry
#endif

.globl start, _start
start:
_start:
multiboot_entry:
#if PAGING
    /** initialize paging table at *INITIAL_PGDIR **/
    movl %eax, %esp         // save multiboot magic

    movl $INITIAL_PGDIR, %edx   // store PGDIR addr
    movl %edx, %edi
    xor  %eax, %eax         // %eax is used for PDE content
    movl $PAGE_BYTES, %ecx
    rep stosb               // fill INITIAL_PGDIR with zeros
    
    movl $(PG_PRESENT | PG_RW | PG_GRAN), %eax
    movl %eax, (%edx)           // entry mapping 0th 4Mb -> 0th 4Mb page

    movl $(KERN_OFF >> PDE_SHIFT), %edi
    movl %eax, (%edx, %edi, PDE_SIZE)  // entry mapping KERN_OFFth 4Mb -> 0th 4Mb page 

    movl %edx, %cr3         // store page directory address
    
    movl %cr4, %eax
    orl  $(CR4_PSE | CR4_PGE), %eax
    movl %eax, %cr4         // enable page-size-ext and global pages

    movl %cr0, %eax
    orl  $(CR0_PG | CR0_WP), %eax
    movl %eax, %cr0         // enable paging and write-protection

    movl %esp, %eax
#endif

    /** initialize the stack **/
    movl $(kern_stack + KERN_STACK_SIZE), %esp

    pushl   $0
    popf                    // clear eflags
    
    pushl   %ebx            // multiboot info structure pointer
    pushl   %eax            // magic
    call    EXT_C(kinit)

hang:   hlt
    jmp hang

.comm kern_stack, KERN_STACK_SIZE
