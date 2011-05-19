#include <kbd.h>
#include <asm.h>

#define KEY_COUNT	128

bool theKeyboard[KEY_COUNT];

kbd_event_f on_press = null;
kbd_event_f on_release = null;

inline bool kbd_state_shift(void) {
	return theKeyboard[0x2A];
}

inline bool kbd_state(uint8_t scan_id) {
	return theKeyboard[scan_id];
}

void kbd_set_onpress(kbd_event_f onpress) {
	on_press = onpress;
}

void kbd_set_oprelease(kbd_event_f onrelease) {
	on_release = onrelease;
}

void irq_keyboard(void *arg) {
	uint8_t scan_code = 0;
	inb(0x60, scan_code);
	if (scan_code & 0x7F) {
		/* on press event */
		theKeyboard[scan_code] = true;
        if (on_press)
            on_press(scan_code);
	}	
	else {
		/* on release event */
		scan_code &= 0x7F;
		theKeyboard[scan_code] = false;
        if (on_release)
            on_release(scan_code);
	}
}

void kbd_setup(void) {
    memset(theKeyboard, 0, sizeof(theKeyboard));
}
