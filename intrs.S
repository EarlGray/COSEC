.text

.extrn intr_handler_table

.equ dummy_errcode  0

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


.globl irq00
irq00:
    

irq_enter:
    pusha
    pushw ds
    pushw es
    movl %eax, *intr_handler_table

isr_enter:
    pusha
    pushw ds
    pushw es
    // call routine

isr_exit:
    pop es
    pop ds
    popa
    pop
    iretd
