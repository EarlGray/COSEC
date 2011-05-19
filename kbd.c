#include <kbd.h>
#include <misc.h>
#include <asm.h>

#define KEY_COUNT	128

bool theKeyboard[KEY_COUNT];

inline bool kbd_state_shift(void) {
	return theKeyboard[0x2A];
}

inline bool kbd_state(uint8_t scan_id) {
	return theKeyboard[scan_id];
}

void kbd_set_onpress(kbd_event_f onpress) {
	
}

void kbd_set_oprelease(kbd_event_f onrelease) {
	
}

void irq_keyboard(void *arg) {
	k_printf("#INT1 kbd\n");
	uint8_t scan_code = 0;
	inb(0x60, scan_code);
	if (scan_code & 0x7F) {
		/* on press event */
		theKeyboard[scan_code] = true;
	}	
	else {
		/* on release event */
		scan_code &= 0x7F;
		theKeyboard[scan_code] = false;
	}
}

