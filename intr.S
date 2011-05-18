#define ASM
/* 
 *      This file contains most of assembly routines used, most of them
 *  are interrupts and exections entry points now.
 */

.data
/* temporary 48bit structure for IDT/GDT loading */
dtreg:
.word 0
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
.global gdt_load
gdt_load:
    movw 4(%esp), %ax
    movw %ax, dtreg
    movl 8(%esp), %eax
    movl %eax, dtreg + 2

    lgdt dtreg
    
    movw $0x0010, %dx
    movw %dx, %ds
    movw %dx, %es
    movw %dx, %fs
    movw %dx, %gs
    
    ljmp $0x0008, $reload_gdt
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
    movw 4(%esp), %ax
    movw %ax, dtreg
    movl 8(%esp), %eax
    movl %eax, dtreg + 2

    lidt dtreg
    ret


/********** ENTRY macros *************/

.macro INTR_PROLOG
    pushf
    pusha
    subl $4, %esp   
    movl %esp, (%esp)
.endm

.macro INTR_END
    addl $4, %esp
    popa
    popf
.endm

.macro ENTRY_NOERR name, handler
.extern \handler
.global \name
\name :
    INTR_PROLOG
    call \handler    
    INTR_END
    iret
.endm

.macro ENTRY_ERR name, handler
.extern \handler
.global \name
\name :
    INTR_PROLOG
    call \handler
    INTR_END
    addl $4, %esp    /* pop errcode */
    iret
.endm

.macro ENTRY_IRQ name, handler
.global \name
\name :
    INTR_PROLOG
    call \handler
    INTR_END
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
.extern irq
ENTRY_IRQ   irq00, irq
ENTRY_IRQ   irq01, irq+4
ENTRY_IRQ   irq02, irq+8
ENTRY_IRQ   irq03, irq+12
ENTRY_IRQ   irq04, irq+16
ENTRY_IRQ   irq05, irq+20
ENTRY_IRQ   irq06, irq+24
ENTRY_IRQ   irq07, irq+28
ENTRY_IRQ   irq08, irq+32
ENTRY_IRQ   irq09, irq+36
ENTRY_IRQ   irq0A, irq+40
ENTRY_IRQ   irq0B, irq+44
ENTRY_IRQ   irq0C, irq+48
ENTRY_IRQ   irq0D, irq+52
ENTRY_IRQ   irq0E, irq+56
ENTRY_IRQ   irq0F, irq+60

