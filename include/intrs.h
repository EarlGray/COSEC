#ifndef __INTRS_H
#define __INTRS_H

#define I8259A_BASE_VECTOR  0x20
#define SYS_INT             0x80

typedef void (*intr_handler_f) (void *);

extern intr_handler_f intr_handler_table[];

void intrs_setup(void);

#endif // __INTRS_H
