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
// IRQ handlers
intr_handler_f irq[16];

// interrupt handlers
void int_dummy(void *);
void int_syscall(void *);
void int_odd_exception(void *);
void irq_stub(void *);
void irq_slave(void *);

void int_division_by_zero(void );
void int_nonmaskable(void );
void int_invalid_op(void *);
void int_gpf(void *);
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

inline void irq_eoi(void) {
    outb_p(0x20, 0x20);
}

void irq_handler(uint32_t irq_num) {
    if (irq_num != 0) k_printf("#IRQ%x ", irq_num);
    intr_handler_f callee = irq[irq_num];
    callee(0);
    irq_eoi();
}

inline void irq_set_handler(uint32_t irq_num, intr_handler_f handler) {
    irq[irq_num] = handler;
}

/****************** IRQs ***********************/

void irq_stub(void *stack) {
    //
}

void irq_slave(void *stack) {
    irq_eoi();
}

void irq_timer(void *arg) {
    static uint32_t counter = 0;
    ++counter;
    if (0 == (counter % 100)) 
        k_printf("#IRQ0 Timer: %d\n", counter);
}

/**************** exceptions *****************/

void int_dummy(void *stack) {
    k_printf("INTR: dummy interrupt\n");
}

void int_syscall(void *stack) {
    k_printf("#SYS");
}

extern void print_mem(uint32_t p, size_t size);

void int_odd_exception(void *stack) {
    k_printf("+");         //"INTR: odd exception\n");

    /*volatile uint32_t a;
    asm(" movl %%esp, %0 \n" : "=r"(a) : :);
    for (a = 0; a < 40; ++a);
    k_printf("Stack at 0x%x\n", a); // (uint32_t)stack);
    print_mem((uint32_t)stack, 0x10);
    thread_hang();      */
}

void int_double_fault(void *stack) {
    k_printf("#DF\nDouble fault...\n");
    thread_hang();
}

void int_division_by_zero(void ) {
    k_printf("#DE\nINTR: division by zero\n");
}

void int_nonmaskable(void ) {
    k_printf("#NM");
}

extern void *theIDT;

void int_invalid_op(void *stack) {
    k_printf("\n#UD\n");
    k_printf("Interrupted at 0x%x : 0x%x\n",    
                *((uint32_t *)stack + 11), 
                *((uint32_t *)stack + 10) );
    print_mem(stack, 0x30);
    //print_mem(theIDT, 0x80);
    thread_hang(); 
}

void int_page_fault(void ) {
    k_printf("#PF");
}

void int_gpf(void *stack) {
    k_printf("#GP\nGeneral protection fault\n");
    k_printf("Interrupted at 0x%x : 0x%x\n", 
                *((uint32_t *)stack + 11), 
                *((uint32_t *)stack + 10) );
    thread_hang();
}


#include <kbd.h>

void intrs_setup(void) {
    //  remap interrupts   
    irq_remap(I8259A_BASE, I8259A_BASE + 8);

    // prepare handler table
    int i;
    for (i = 0; i < 8; ++i)
        irq[i] = irq_stub;
    for (i = 8; i < 16; ++i)
        irq[i] = irq_slave;

    irq_set_handler(0, irq_timer);
    irq_set_handler(1, irq_keyboard);

    idt_setup();
    idt_deploy();
}
