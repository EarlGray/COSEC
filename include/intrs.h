#ifndef __INTRS_H
#define __INTRS_H

typedef void (*intr_handler_f) (void *);

extern intr_handler_f intr_handler_table[];

void set_trap_gate(int intrnum, intr_handler_f handler);
void set_call_gate(int intrnum, intr_handler_f handler);
void set_intr_gate(int intrnum, intr_handler_f handler);

void intrs_setup(void);

#endif // __INTRS_H
