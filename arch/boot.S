#define NOT_CC
/*
#define __ELF__  
*/

#include <arch/multiboot.h>
#include <mm/paging.h>
#include <globl.h>

#define STACK_SIZE  0x00000800

#define CR4_PSE     0x00000010
#define CR4_PGE     0x00000080

#define CR0_PG      0x80000000
#define CR0_WP      0x00010000

.globl _start
.globl start

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

start:
_start:
multiboot_entry:
    /** initialize paging table at *INITIAL_PGDIR **/
    movl %eax, %esp         // save multiboot magic

    movl $INITIAL_PGDIR, %edx   // store PGDIR addr
    movl %edx, %edi
    xor  %eax, %eax         // %eax is used for PDE content
    movl $N_PDE, %ecx
    rep stosl               // fill INITIAL_PGDIR with zeros
    
    movl $(PG_PRESENT | PG_RW | PG_GRAN), %eax
    movl %eax, (%edx)           // entry mapping 0th 4Mb -> 0th 4Mb page

    movl $(KERN_OFF >> PDE_SHIFT), %edi
    movl %eax, (%edx, %edi, PDE_SIZE)  // entry mapping KERN_OFFth 4Mb -> 0th 4Mb page 

    movl %edx, %cr3         // store page directory address
    
    movl %cr4, %eax
    orl  $(CR4_PSE | CR4_PGE), %eax
    movl %eax, %cr4         // enable page-size-ext and global pages

#if PAGING
    movl %cr0, %eax
    orl  $(CR0_PG | CR0_WP), %eax
    movl %eax, %cr0         // enable paging and write-protection
#endif
    movl %esp, %eax

    /** initialize the stack **/
    movl $(kern_stack + STACK_SIZE), %esp

    pushl   $0
    popf                    // clear eflags
    
    pushl   %ebx            // multiboot info structure pointer
    pushl   %eax            // magic
    call    EXT_C(kmain)

hang:   hlt
    jmp hang

.comm kern_stack, STACK_SIZE
