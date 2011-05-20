#include <kbd.h>
#include <asm.h>
#include <intrs.h>

#define KEY_COUNT   	128

/*************** Keyboard buffer ****************/

#define KBD_BUF_SIZE    256 
struct {
    uint8_t head;
    uint8_t tail;
    scancode_t buf[KBD_BUF_SIZE];
} theKbdBuf; 

inline void kbd_buf_setup(void) {
    theKbdBuf.head = theKbdBuf.tail = 0;
}

scancode_t kbd_pop_scancode(void) {
    if (theKbdBuf.head == theKbdBuf.tail) 
        return 0;
    else if (theKbdBuf.tail != KBD_BUF_SIZE-1)
        return theKbdBuf.buf[theKbdBuf.tail++];
    else {
        theKbdBuf.tail = 0;
        return theKbdBuf.buf[KBD_BUF_SIZE-1];
    }
}

static void kbd_push_scancode(scancode_t scan_id) {
    if (theKbdBuf.tail - theKbdBuf.head != 1) {    // if there is room for a new scancode
        if (theKbdBuf.head != KBD_BUF_SIZE-1)      // if append not at border of buf
            theKbdBuf.buf[ ++theKbdBuf.head ] = scan_id;
        else if (theKbdBuf.tail != 0) 
            theKbdBuf.buf [ theKbdBuf.head = 0 ] = scan_id;
    }
}


/***************************
              layout
    *****************************/

struct scan_key {
    char normal;
    char shift;
    char ctrl;
};

struct kbd_layout {
    struct scan_key key[KEY_COUNT];
};

/** Default qwerty-ASCII translator **/
const struct kbd_layout qwerty_layout = { 
    .key = {
        { 0, 0, 0 },        { 0, 0, 0},         {'1', '!', 0 },     { '2', '@', 0 }, // 0
        { '3', '#', 0 },    { '4', '$', 0 },    { '5', '%', 0 },    { '6', '^', 0 }, // 4
        { '7', '&', 0 },    { '8', '*', 0 },    { '9', '(', 0 },    { '0', ')', 0 }, // 8
        { '-', '_', 0 },    { '=', '+', 0 },    { '\b', '\b', 0 },  { 0, 0, 0 },     // 0xC
        { 'q', 'Q', 0 },    { 'w', 'W', 0 },    { 'e', 'E', 0 },    { 'r', 'R', 0 }, // 0x10
        { 't', 'T', 0 },    { 'y', 'Y', 0 },    { 'u', 'U', 0 },    { 'i', 'I', 0 },  // 0x14
        { 'o', 'O', 0 },    { 'p', 'P', 0 },    { '[', '{', 0 },    { ']', '}', 0 },  // 0x18
        { '\n', '\n', 0 },  { 0, 0, 0 },        { 'a', 'A', 1 },    { 's', 'S', 0 },  // 0x1C
        { 'd', 'D', 0 },    { 'f', 'F', 0 },    { 'g', 'G', 0 },    { 'h', 'H', 0 },  // 0x20
        { 'j', 'J', 0 },    { 'k', 'K', 0 },    { 'l', 'L', 0 },    { ';', ':', 0 },  // 0x24
        { '\'', '"', 0 },   { 0, 0, 0 },        { 0, 0, 0 },        { '\\', '|', 0 },  // 0x28
        { 'z', 'Z', 0 },    { 'x', 'X', 0 },    { 'c', 'C', 0 },    { 'v', 'V', 0 },  // 0x2C
        { 'b', 'B', 0 },    { 'n', 'N', 0 },    { 'm', 'M', 0 },    { ',', '<', 0 },  // 0x30
        { '.', '>', 0 },    { '/', '?', 0 },    { 0, 0, 0 },        { 0, 0, 0 },  // 0x34
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x38
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x3C

        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x40
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x44
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x48
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x4C
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x50
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x54
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x58
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x5C
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x60
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x64
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x68
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x6C
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x70
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x74
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x78
        { 0, 0, 0 }, { 0, 0, 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x7C
    }
};

char translate_from_scan(struct kbd_layout *layout, scancode_t scan_code) {
    if (layout == null) layout = &qwerty_layout;

    if (kbd_state_ctrl()) 
        return layout->key[scan_code].ctrl;
    if (kbd_state_shift())
        return layout->key[scan_code].shift;
    return layout->key[scan_code].normal;
}

/************* Keyboard driver ******************/

bool theKeyboard[KEY_COUNT];

kbd_event_f on_press = null;
kbd_event_f on_release = null;

inline bool kbd_state_shift(void) {
	return theKeyboard[0x2A] | theKeyboard[0x36];
}

inline void kbd_state_ctrl(void) {
    return theKeyboard[0x1D]; //| theKeyboard[0x ]
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

void keyboard_irq(void *arg) {
	uint8_t scan_code = 0;
	inb(0x60, scan_code);
	if (scan_code & 0x7F) {
		/* on press event */
		theKeyboard[scan_code] = true;
        kbd_push_scancode(scan_code);
        if (on_press)
            on_press(scan_code);
        k_printf("0x%x\n", (uint32_t)scan_code);
	}	
	else {
		/* on release event */
		scan_code &= 0x7F;
		theKeyboard[scan_code] = false;
        kbd_push_scancode(scan_code);
        if (on_release)
            on_release(scan_code);
	}
}

void kbd_setup(void) {
    memset(theKeyboard, 0, sizeof(theKeyboard));
    kbd_buf_setup();
    irq_set_handler(1, keyboard_irq);
}
