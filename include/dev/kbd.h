#ifndef __KBD_H__
#define __KBD_H__

typedef uint8_t scancode_t;

bool kbd_state_shift(void);
bool kbd_state_ctrl(void);
bool kbd_state(scancode_t scan_id);

scancode_t kbd_wait_scan(void);

/************  events  ***************/

typedef void (*kbd_event_f)(scancode_t);

void kbd_set_onpress(kbd_event_f onpress);
void kbd_set_oprelease(kbd_event_f onrelease);


/************  buffer   **************/

scancode_t kbd_pop_scancode(void);
void kbd_buf_clear(void);

/************  layouts  *************/
struct kbd_layout;

// if layout is null, uses default
char translate_from_scan(const struct kbd_layout *layout, scancode_t scan_code);

void keyboard_irq();
void kbd_setup(void);

#endif //__KBD_H__
