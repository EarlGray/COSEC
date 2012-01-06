#define NOT_CC
/*
#define __ELF__  
*/

#include <arch/multiboot.h>
#include <globl.h>
#define STACK_SIZE  0x00000800

.globl _start
.globl start

.text
start:
_start:
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

.extern thePageDirectory
.extern first_page_table

multiboot_entry:
    movl $(kern_stack + STACK_SIZE), %esp
    
    // initialize paging structures 
    
    // enable paging

    pushl   $0
    popf                    // clear eflags
    
    //addl    $VIRTUAL_OFFSET, %ebx
    pushl   %ebx            // multiboot info structure pointer
    pushl   %eax            // magic
    call    EXT_C(kmain)

hang:   hlt
    jmp hang

.comm kern_stack, STACK_SIZE
