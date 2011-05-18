#ifndef __INTRS_H
#define __INTRS_H

#define I8259A_BASE 0x20
#define SYS_INT     0x80

#ifndef ASM

typedef void (*intr_handler_f) (void *);

extern intr_handler_f intr_handler_table[];

void intrs_setup(void);

#endif
#endif // __INTRS_H
