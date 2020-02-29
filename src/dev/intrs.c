/*
 *      This file contains high-level interupts handlers
 *  Now it also includes PIC-related code, though it would be nice to put it
 *  to its own header, but I don't want mupltiply files now
 *  TODO: separate pic.c
 *      About interrupt handling: 
 *  exceptions are handled directly by one of int_foo() functions
 *  IRQs are handled by irq_hander(), which calls a handler registered in irq[]
 */


#include <arch/i386.h>

#include <syscall.h>
#include <dev/intrs.h>

#include <mem/paging.h>

#include <stdio.h>
#include <sys/errno.h>

#if INTR_DEBUG
# define __DEBUG
#endif
#include <cosec/log.h>

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
// IRQ handlers
intr_handler_f irq[16];

volatile uint32_t irq_happened[16] = { 0 };

/*
 *    Implementations
 */

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

    outb_p(PIC1_DATA_PORT, 4); // ICW3: Master to expect Slave at pin 2
    outb_p(PIC2_DATA_PORT, 2); // ICW3: Slave to be aware it's IRQ2 at Master

    outb_p(PIC1_DATA_PORT, ICW4_I8086);
    outb_p(PIC2_DATA_PORT, ICW4_I8086);

    // restore mask
    outb(PIC1_DATA_PORT, master_mask);
    outb(PIC2_DATA_PORT, slave_mask);
}

static inline void irq_mask(irqnum_t irq_num, bool set) {
    uint8_t mask;
    uint16_t port = PIC1_DATA_PORT;
    if (irq_num >= 8) {
        port = PIC2_DATA_PORT;
        irq_num -= 8;
    }
    inb(port, mask);
    if (set) mask &= ~(1 << irq_num);
    else mask |= (1 << irq_num);
    outb(port, mask);
}

void irq_enable(irqnum_t n) {
    irq_mask(n, true);
}

void irq_disable(irqnum_t n) {
    irq_mask(n, false);
}


uint16_t irq_get_mask(void) {
    uint16_t res = 0;
    uint8_t mask = 0;
    inb(PIC1_DATA_PORT, mask);
    res = mask;
    mask = 0;
    inb(PIC2_DATA_PORT, mask);
    res |= (mask << 8);
    return res;
}

inline void irq_eoi(uint32_t irq_num) {
    uint32_t port = (irq_num < 8 ? PIC1_CMD_PORT : PIC2_DATA_PORT);
    outb_p(port, PIC_EOI);
}

void irq_handler(void *stack, uint32_t irq_num) {
    /*
    if (irq_num > 0)
        logmsgf("%s(%d)\n", __FUNCTION__, irq_num);
        */
    irq_happened[irq_num] += 1;
    intr_handler_f callee = irq[irq_num];
    callee((void *)cpu_stack());
    irq_eoi(irq_num);
}

inline void irq_set_handler(irqnum_t irq_num, intr_handler_f handler) {
    irq[irq_num] = handler;
}

/****************** IRQs ***********************/

void irq_stub() {
    logmsgf("irq_stubA(), shouldn't happen\n");
}

void irq_slave() {
    logmsgf("irq_stubB(), shouldn't happen\n");
}

/**************** exceptions *****************/

void int_dummy() {
    logmsg("exception: #DUMMY\n");
}

void int_odd_exception() {
    logmsg("exception: #ODD\n");
}

void int_double_fault() {
    panic("DOUBLE FAULT");
}

void int_division_by_zero() {
    logmsgef("INTR: division by zero, TODO\n");
    cpu_hang();
}

void int_nonmaskable() {
    logmsg("exception: #NONMASKABLE\n");
}

void int_invalid_op(void *stack) {
    char buf[80];
    snprintf(buf, 80, "exception: #UD at %.8x:%0.8x",
                (uint) *((uint32_t *)stack + 11),
                (uint) *((uint32_t *)stack + 10) );
    panic(buf);
}


void int_page_fault(uint32_t *stack, err_t err) {
    pg_fault(stack, err);
}

void int_segment_not_present(void) {
    logmsg("#NP");
}

void int_overflow() {
    logmsg("interrupt: overflow\n");
}

void int_breakpoint(void) {
    logmsg("interrupt: breakpoint\n");
}


void int_out_of_bounds(void ) {
    logmsg("interrupt: out of bounds\n");
}

void int_stack_segment(void ) {
    logmsg("interrupt: stack segment\n");
}

void int_invalid_tss(void ) {
    logmsg("interrupt: invalid tss\n");
    cpu_hang();
}

void int_gpf(uint32_t *stack, err_t err) {
    uint32_t eip = stack[1];
    uint32_t cs = stack[2];

    logmsgef("\nFatal: General Protection Fault at *%x:%x, error_code 0x%x\n",
            cs, eip, err);
    cpu_hang();
}


void intrs_setup(void) {
    // remap interrupts   
    irq_remap(I8259A_BASE, I8259A_BASE + 8);

    // prepare handler table
    int i;
    for (i = 0; i < 8; ++i) {
        irq_disable(i);
        irq[i] = irq_stub;
    }
    for (i = 8; i < 16; ++i) {
        irq_disable(i);
        irq[i] = irq_slave;
    }

    // slave PIC must be accessible:
    irq_enable(NMI_IRQ);
}

int irq_wait(irqnum_t irqnum) {
    return_err_if(irqnum >= 16, -EINVAL, "Wrong IRQ number");

    uint32_t n = irq_happened[irqnum];
    while (n >= irq_happened[irqnum]) {
        cpu_halt();
    }
    return 0;
}
