#define ASM
/* 
 *      This file contains most of assembly routines used, most of them
 *  are interrupts and exections entry points now.
 */

.equ dummy_errcode, 0
.extern intr_handler_table

/************ Handlers **************/
.extern int_syscall
.extern int_dummy
.extern int_odd_exception
.extern irq_stub

/* temporary 48bit structure for IDT/GDT loading */
dtreg:
.word 0
.long 0

.text

.global gdt_load
/* */
gdt_load:
    movw 4(%esp), %ax
    movw %ax, dtreg
    movl 8(%esp), %eax
    movl %eax, dtreg + 2

    lgdt dtreg
    ret

.global idt_load
/* */
idt_load:
    movw 4(%esp), %ax
    movw %ax, dtreg
    movl 8(%esp), %eax
    movl %eax, dtreg + 2

    lidt dtreg
    ret

.global dummyentry
/*  Every unrecognized software interrupt  */
dummyentry:
    call int_dummy
    
.global syscallentry
syscallentry:
    call int_syscall

/* Intel is shit and encourages indian coders to multiply code */
/************** Exceptions *****************/
.globl isr00
isr00:

.globl isr01
isr01:

.globl isr02
isr02:

.globl isr03
isr03:

.globl isr04
isr04:

.globl isr05
isr05:

.globl isr06
isr06:

.globl isr07
isr07:

.globl isr08
isr08:

.globl isr09
isr09:

.globl isr0A
isr0A:

.globl isr0B
isr0B:

.globl isr0C
isr0C:

.globl isr0D
isr0D:

.globl isr0E
isr0E:

.globl isr0F
isr0F:

.globl isr10
isr10:

.globl isr11
isr11:

.globl isr12
isr12:

.globl isr13
isr13:

.global isr14to1F
isr14to1F:
    cli
    subl 4, %esp
    movl %esp, (%esp)
    call int_odd_exception

/************ IRQS  **************/

.global irq00
irq00:

.global irq01
irq01:

.globl irq02
irq02:
    
.global irq03
irq03:


.global irq04
irq04:

.global irq05
irq05:

.global irq06
irq06:

.global irq07
irq07:

.global irq08
irq08:
.global irq09
irq09:
.global irq0A
irq0A:
.global irq0B
irq0B:
.global irq0C
irq0C:
.global irq0D
irq0D:
.global irq0E
irq0E:
.global irq0F
irq0F:
    cli
    subl 4, %esp
    call irq_stub


irq_enter:
    pusha
    pushw %ds
    pushw %es
    movl %eax, (intr_handler_table)

isr_enter:
    pusha
    pushw %ds
    pushw %es
    // call routine

isr_exit:
    pop %es
    pop %ds
    popa
    //po
    iret
