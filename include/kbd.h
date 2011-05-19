#ifndef __KBD_H__
#define __KBD_H__

#include <misc.h>

typedef void (*kbd_event_f)(uint8_t);

bool kbd_state_shift(void);
bool kbd_state(uint8_t scan_id);

void kbd_set_onpress(kbd_event_f onpress);
void kbd_set_oprelease(kbd_event_f onrelease);

void irq_keyboard(void *);

void kbd_setup(void);

#endif //__KBD_H__
