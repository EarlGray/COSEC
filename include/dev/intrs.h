#ifndef __INTRS_H
#define __INTRS_H

#define I8259A_BASE 0x20
#define SYS_INT     0x80

#ifndef ASM

typedef void (*intr_handler_f) (void *);

uint16_t irq_get_mask(void);
void irq_mask(bool set, uint8_t irq_num);

void intrs_setup(void);

void irq_set_handler(uint32_t irq_num, intr_handler_f handler);

#endif
#endif // __INTRS_H
