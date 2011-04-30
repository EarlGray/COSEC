/*
 *      This file contains high-level interupts handlers
 *  Now it also includes PIC-related code, though it would be nice to put it
 *  to its own header, but I don't wnat mupltiply files now
 *  TODO: separate pic.c
 */

#include <misc.h>
#include <asm.h>

#include <intrs.h>
#include <memory.h>
#include <idt.h>

#define ICW1_ICW4       0x01
#define ICW1_SINGLE     0x02        // 0x00 - cascade
#define ICW1_INTERVAL   0x04        // 0x00 - 
#define ICW1_LEVEL      0x08        // triggered (edged) mode
#define ICW1_INIT       0x10        // reqired

#define ICW4_I8086      0x01        // i8086 (i8080) mode
#define ICW4_AUTO       0x02        // auto (normal) EOI
#define ICW4_BUF_SLAVE  0x08        // buffered mode/slave
#define ICW4_BUF_MASTER 0x0C        // buffered mode/master
#define ICW4_SFNM       0x10        // special fully nested (not)

#define PIC1_CMD_PORT    0x20
#define PIC1_DATA_PORT   (PIC1_CMD_PORT + 1)
#define PIC2_CMD_PORT    0xA0
#define PIC2_DATA_PORT   (PIC2_CMD_PORT + 1)

#define PIC_EOI         0x20            // PIC end-of-interrput command code

/* 
 *      Declarations
 */

// interrupt handlers
void int_dummy(void *);
void int_syscall(void *);
void int_odd_exception(void *);
void irq_stub(void *);

/* 
 *    Implementations
 */

intr_handler_f intr_handler_table[IDT_SIZE];

static void 
irq_remap(uint8_t master, uint8_t slave) {
    uint8_t master_mask, slave_mask;

    // save masks
    inb(PIC1_DATA_PORT, master_mask);
    inb(PIC2_DATA_PORT, slave_mask);

    // ICWs
    outb_p(PIC1_CMD_PORT, ICW1_INIT | ICW1_ICW4);
    outb_p(PIC2_CMD_PORT, ICW1_INIT | ICW1_ICW4);

    outb_p(PIC1_DATA_PORT, master);
    outb_p(PIC2_DATA_PORT, slave);

    outb_p(PIC1_DATA_PORT, 4);
    outb_p(PIC2_DATA_PORT, 2);

    outb_p(PIC1_DATA_PORT, ICW4_I8086);
    outb_p(PIC2_DATA_PORT, ICW4_I8086);

    // restore mask
    outb(PIC1_DATA_PORT, master_mask);
    outb(PIC2_DATA_PORT, slave_mask);
}

void intrs_setup(void) {
    //  remap interrupts   
    irq_remap(I8259A_BASE_VECTOR, I8259A_BASE_VECTOR + 8);

    // prepare handler table

    idt_setup();
    idt_deploy();
}

void int_dummy(void *stack) {
    k_printf("INTR: dummy interrupt\n");
    intrs_disable();
    thread_hang();
}

void int_syscall(void *stack) {
    k_printf("INTR: syscall]n");
    intrs_disable();
    thread_hang();
}

void int_odd_exception(void *stack) {
    k_printf("INTR: odd exception\n");
    intrs_disable();
    thread_hang();
}

void irq_stub(void *stack) {
    k_printf("IRQ\n");
    thread_hang();
}
