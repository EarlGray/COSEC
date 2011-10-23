#ifndef __INTRS_H
#define __INTRS_H

#define I8259A_BASE 0x20
#define SYS_INT     0x80

#define TIMER_IRQ   0
#define PS2_IRQ     1
#define NMI_IRQ     2
#define COM2_IRQ    3
#define COM1_IRQ    4

#ifndef ASM

typedef void (*intr_handler_f) (void *);

uint16_t irq_get_mask(void);
void irq_mask(uint8_t irq_num, bool set);

void intrs_setup(void);

void irq_set_handler(uint8_t irq_num, intr_handler_f handler);

void * intr_stack_ret_addr(void);

#endif
#endif // __INTRS_H
