#define ASM (1)
/*
#define __ELF__  
*/

#include <multiboot.h>
#define STACK_SIZE  0x00004000

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

multiboot_entry:
    movl    $(stack + STACK_SIZE),  %esp

    pushl   $0
    popf
    
    pushl   %ebx            // multiboot info structure pointer
    pushl   %eax            // magic
    call    EXT_C(kmain)

hang:   hlt
    jmp hang

.comm stack, STACK_SIZE
