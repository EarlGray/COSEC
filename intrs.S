.text

.extrn intr_handler_table

.globl isr00
isr00:
    cli
    pushl $0        //  dummy errcode
    pushl $00       //  interrupt number
    jmp isr_enter

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
