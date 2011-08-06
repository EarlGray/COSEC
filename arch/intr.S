#define ASM

#include <globl.h>

#define KERN_DS     $0x0010
#define KERN_CS     $0x0008
/* 
 *      This file contains most of assembly routines used, most of them
 *  are interrupts and exections entry points now.
 */

.data
/* temporary 48bit structure for IDT/GDT loading */
dtreg:
.word 0
.long 0

/* points to a stack position where last cc:eip has been saved */
intr_ret:
.long 0

/********************** text *****************************/
.text
.align 4

/************ Handlers ***************/
.extern int_handlers_table

.extern int_syscall
.extern int_dummy
.extern int_odd_exception
.extern irq_stub

/******** Tables-related *************/

.macro DTREG_LOAD
    movw 4(%esp), %ax
    movw %ax, dtreg
    movl 8(%esp), %eax
    movl %eax, dtreg + 2
.endm 

.macro SHAKE_REGS
    movw KERN_DS, %dx
    movw %dx, %ds
    movw %dx, %es
    movw %dx, %fs
    movw %dx, %gs
.endm


.global gdt_load

gdt_load:
    DTREG_LOAD
    lgdt dtreg

    SHAKE_REGS  
    ljmp KERN_CS, $reload_gdt
reload_gdt:
    ret

.global gdt_get
gdt_get:
    movl 4(%esp), %esi
    sgdt dtreg
    movw dtreg, %ax
    movw %ax, (%esi)
    movl dtreg+2, %eax
    movl %eax, 2(%esi)
    ret

.global idt_load
idt_load:
    DTREG_LOAD
    lidt dtreg

    ret

.global intr_stack_ret_addr
intr_stack_ret_addr:
    movl intr_ret, %eax
    ret

/********** ENTRY macros *************/

.macro INTR_PROLOG
    /* eflags already saved by CPU as (uint)(intr_ret[2]) */
    cli
    pusha
    subl $4, %esp   
.endm

.macro INTR_END
    addl $4, %esp
    movl 8(%esp), %edi
    movl %edi, intr_ret
    popa
.endm

.macro ENTRY_NOERR name, handler
.extern \handler
.global \name
\name :
    INTR_PROLOG
    movl %esp, (%esp)
    call \handler    
    INTR_END
    iret
.endm

.macro ENTRY_ERR name, handler
.extern \handler
.global \name
\name :
    INTR_PROLOG
    movl %esp, (%esp)
    call \handler
    INTR_END
    addl $4, %esp    /* pop errcode */
    iret
.endm


#if INTR_PROFILING
start_tick: 
.long   0
.long   0

.globl intr_start_rdtsc
intr_start_rdtsc:
    movl 4(%esp), %ebx
    movl start_tick, %eax
    movl %eax, (%ebx)
    movl start_tick + 4, %eax
    movl %eax, 4(%ebx)
    ret

.extern intr_profile

.macro INTR_PROFILING_START
    rdtsc
    movl %eax, start_tick
    movl %edx, start_tick + 4
.endm

.macro INTR_PROFILING_END
    subl $8, %esp
    rdtsc
    movl %edx, 4(%esp)
    movl %eax, (%esp)
    call intr_profile
    addl $8, %esp
.endm
#else
#   define INTR_PROFILING_START
#   define INTR_PROFILING_END
#endif

.macro ENTRY_IRQ name, num
.global \name
\name :
    INTR_PROLOG 

    /* save an interrupt's point of support in the stack */
    lea 0x24(%esp), %ebx
    movl %ebx, intr_ret

    INTR_PROFILING_START

    /* call the handler */
    movl \num, (%esp)
    call irq_handler

    INTR_PROFILING_END  

    INTR_END
    movl -0x14(%esp), %esp
    iret
.endm

/******** multiple entries ***********/

ENTRY_NOERR dummyentry, int_dummy           /* unrecognized software interrupt   */
ENTRY_NOERR syscallentry, int_syscall       /* sys int                           */
ENTRY_NOERR isr14to1F, int_odd_exception    /* unused exceptions                 */

/************* exceptions ************/
ENTRY_NOERR isr00, int_division_by_zero
ENTRY_NOERR isr01, int_odd_exception
ENTRY_NOERR isr02, int_nonmaskable
ENTRY_NOERR isr03, int_odd_exception
ENTRY_NOERR isr04, int_odd_exception
ENTRY_NOERR isr05, int_odd_exception
ENTRY_NOERR isr06, int_invalid_op
ENTRY_NOERR isr07, int_odd_exception
ENTRY_ERR   isr08, int_double_fault
ENTRY_NOERR isr09, int_odd_exception
ENTRY_ERR   isr0A, int_odd_exception
ENTRY_ERR   isr0B, int_odd_exception
ENTRY_ERR   isr0C, int_odd_exception
ENTRY_ERR   isr0D, int_gpf
ENTRY_ERR   isr0E, int_page_fault
ENTRY_NOERR isr0F, int_odd_exception
ENTRY_NOERR isr10, int_odd_exception
ENTRY_NOERR isr11, int_odd_exception
ENTRY_NOERR isr12, int_odd_exception
ENTRY_NOERR isr13, int_odd_exception

/************ IRQS  **************/
.extern irq_handler
ENTRY_IRQ   irq00, $0x0
ENTRY_IRQ   irq01, $0x1
ENTRY_IRQ   irq02, $0x2
ENTRY_IRQ   irq03, $0x3
ENTRY_IRQ   irq04, $0x4
ENTRY_IRQ   irq05, $0x5
ENTRY_IRQ   irq06, $0x6
ENTRY_IRQ   irq07, $0x7
ENTRY_IRQ   irq08, $0x8
ENTRY_IRQ   irq09, $0x9
ENTRY_IRQ   irq0A, $0xA
ENTRY_IRQ   irq0B, $0xB
ENTRY_IRQ   irq0C, $0xC
ENTRY_IRQ   irq0D, $0xD
ENTRY_IRQ   irq0E, $0xE
ENTRY_IRQ   irq0F, $0xF

