#ifndef __INTRS_H
#define __INTRS_H

typedef void (*intr_handler_f) ();

extern intr_handler_f intr_handler_table[];

void intrs_setup(void);

#endif // __INTRS_H
