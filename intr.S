#define ASM
/* 
 *      This file contains most of assembly routines used, most of them
 *  are interrupts and exections entry points now.
 */

.text
.extern intr_handler_table
/* temporary 48bit structure for IDT/GDT loading */
dtreg:
.word 0
.long 0

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
    movl %eax, dtreg

    lidt dtreg
    ret

.equ dummy_errcode, 0

.globl isr00
isr00:
    cli
    pushl $dummy_errcode
    pushl $00       //  interrupt number
    jmp isr_enter

.globl isr01
isr01:
    cli
    pushl $dummy_errcode
    pushl $01
    jmp isr_enter

.globl isr02
isr02:


.globl irq00
irq00:
    

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
