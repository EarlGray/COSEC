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

#include <dev/intrs.h>

#include <mem/paging.h>

#include <std/stdio.h>

#include <log.h>

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

#if INTR_DEBUG
#   define intr_logf(msg, ...) logf(msg, __VA_ARGS__)
#   define intr_log(msg) log(msg)
#else
#   define intr_logf(msg, ...)
#   define intr_log(msg)
#endif


/* 
 *      Declarations
 */
// IRQ handlers
intr_handler_f irq[16];

// interrupt handlers
void int_dummy();
void int_syscall();
void int_odd_exception();
void irq_stub();
void irq_slave();

void int_division_by_zero(void );
void int_nonmaskable(void );
void int_invalid_op(void *);
void int_gpf(void );
void int_page_fault(void );

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

    outb_p(PIC1_DATA_PORT, 4);
    outb_p(PIC2_DATA_PORT, 2);

    outb_p(PIC1_DATA_PORT, ICW4_I8086);
    outb_p(PIC2_DATA_PORT, ICW4_I8086);

    // restore mask
    outb(PIC1_DATA_PORT, master_mask);
    outb(PIC2_DATA_PORT, slave_mask);
}

void irq_mask(uint8_t irq_num, bool set) {
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

uint16_t irq_get_mask(void) {
    uint16_t res = 0;
    uint8_t mask;
    inb(PIC1_DATA_PORT, mask);
    res = mask;
    inb(PIC2_DATA_PORT, mask);
    res |= (mask << 8);
    return res;
}

inline void irq_eoi(void) {
    outb_p(0x20, 0x20);
}

void irq_handler(uint32_t irq_num) {
    intr_handler_f callee = irq[irq_num];
    callee((void *)cpu_stack());
    irq_eoi();
}

inline void irq_set_handler(uint8_t irq_num, intr_handler_f handler) {
    irq[irq_num] = handler;
}

/****************** IRQs ***********************/

void irq_stub() {
}

void irq_slave() {
    irq_eoi();
}

/**************** exceptions *****************/

void int_dummy() {
    intr_log("INTR: dummy interrupt\n");
}

extern void int_syscall();

void int_odd_exception() {
    intr_log("unspecified exception");
}

void int_double_fault() {
    intr_log("#DF\nDouble fault...\n");
    panic("DOUBLE FAULT");
}

void int_division_by_zero(void ) {
    intr_log("#DE\nINTR: division by zero\n");
}

void int_nonmaskable(void ) {
    //intr_log("#NM");
}

void int_invalid_op(void *stack) {
    char buf[80];
    snprintf(buf, 80, "#UD at %.8x:%0.8x",    
                (uint) *((uint32_t *)stack + 11), 
                (uint) *((uint32_t *)stack + 10) );
    panic(buf);
}


void int_page_fault(void ) {
    pg_fault();
}

void int_segment_not_present(void) {
    intr_log("#NP");
}

void int_overflow(void ) {
    intr_log("#OF");
}

void int_breakpoint(void) {
    intr_log("#BP");
}


void int_out_of_bounds(void ) {
    intr_log("#BR");
}

void int_stack_segment(void ) {
    intr_log("#SS");
}

void int_invalid_tss(void ) {
    intr_log("#TS");
    intr_log("Invalid TSS exception\n");
    cpu_hang();
}

void int_gpf(void) {
    intr_logf("#GP\nGeneral protection fault, error code %x\n", 
            intr_err_code());
    uint *ret_eip = (uint *)(intr_context_esp() + 0x2c);
    intr_logf("Interrupted at %x:%x\n", ret_eip[1], ret_eip[0]);
    cpu_hang();
}


void intrs_setup(void) {
    // remap interrupts   
    irq_remap(I8259A_BASE, I8259A_BASE + 8);

    // prepare handler table
    int i;
    for (i = 0; i < 8; ++i) {
        irq_mask(i, false);
        irq[i] = irq_stub;
    }
    for (i = 8; i < 16; ++i) {
        irq_mask(i, false);
        irq[i] = irq_slave;
    }
}
