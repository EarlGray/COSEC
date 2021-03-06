#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <dev/intrs.h>
#include <arch/i386.h>

#include <dev/kbd.h>
#include <cosec/log.h>

#define KEY_COUNT       128

/*************** Keyboard buffer ****************/

#define KBD_BUF_SIZE    256
volatile struct {
    uint8_t head;
    uint8_t tail;
    scancode_t buf[KBD_BUF_SIZE];
} theKbdBuf;


inline void kbd_buf_clear(void) {
    theKbdBuf.head = theKbdBuf.tail = 0;
}

inline void kbd_buf_setup(void) {
    kbd_buf_clear();
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

#define next_circular(var, limit) \
        (((var) != (limit-1)) ? ++(var) : ((var) = 0) )

static void kbd_push_scancode(scancode_t scan_id) {
    if (theKbdBuf.tail == theKbdBuf.head+1)
        next_circular(theKbdBuf.tail, KBD_BUF_SIZE);

    // if append not at border of buf
    theKbdBuf.buf[ next_circular(theKbdBuf.head, KBD_BUF_SIZE) ] = scan_id;
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
        { 0,   0,   0 },    { 0,   0,   0},     {'1', '!', 0 },     { '2', '@', 0 }, // 0
        { '3', '#', 0 },    { '4', '$', 0 },    { '5', '%', 0 },    { '6', '^', 0 }, // 4
        { '7', '&', 0 },    { '8', '*', 0 },    { '9', '(', 0 },    { '0', ')', 0 }, // 8
        { '-', '_', 0 },    { '=', '+', 0 },    { '\b', '\b', 0 },  { 0, 0, 0 },     // 0xC
        { 'q', 'Q', 16 },   { 'w', 'W', 21 },   { 'e', 'E', 5 },    { 'r', 'R', 17 },// 0x10
        { 't', 'T', 19 },   { 'y', 'Y', 23 },   { 'u', 'U', 20 },   { 'i', 'I', 9 }, // 0x14
        { 'o', 'O', 14 },   { 'p', 'P', 15 },   { '[', '{', 0 },    { ']', '}', 0 }, // 0x18
        { '\n', '\n', 0 },  { 0, 0, 0 },        { 'a', 'A', 1 },    { 's', 'S', 18 },// 0x1C
        { 'd', 'D', 4 },    { 'f', 'F', 6 },    { 'g', 'G', 7 },    { 'h', 'H', 8 }, // 0x20
        { 'j', 'J', 10 },   { 'k', 'K', 11 },   { 'l', 'L', 12 },   { ';', ':', 0 }, // 0x24
        { '\'', '"', 0 },   { 0, 0, 0 },        { 0, 0, 0 },        { '\\', '|', 0 }, // 0x28
        { 'z', 'Z', 24 },   { 'x', 'X', 22 },   { 'c', 'C', 3 },    { 'v', 'V', 0 }, // 0x2C
        { 'b', 'B', 2 },    { 'n', 'N', 12 },   { 'm', 'M', 13 },   { ',', '<', 0 }, // 0x30
        { '.', '>', 0 },    { '/', '?', 0 },    { 0, 0, 0 },        { 0, 0, 0 },  // 0x34
        { 0, 0, 0 }, { ' ', ' ', 0 },  { 0, 0, 0 }, { 0, 0, 0 },  // 0x38
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

char translate_from_scan(const struct kbd_layout *layout, scancode_t scan_code) {
    if (scan_code & 0x80)
        return 0;
    if (layout == null)
        layout = &qwerty_layout;

    const struct scan_key *key = (layout->key + scan_code );
    if (kbd_state_ctrl())
        return key->ctrl;
    if (kbd_state_shift())
        return key->shift;
    return key->normal;
}

/*************** getscan   **********************/
volatile scancode_t sc;

static void on_scan(scancode_t b) {
    sc = b;
}

scancode_t kbd_wait_scan(bool release_too) {
    kbd_set_onpress(on_scan);
    if (release_too)
        kbd_set_onrelease(on_scan);
    sc = 0;

    while (sc == 0) cpu_halt();

    kbd_set_onpress(null);
    if (release_too)
        kbd_set_onrelease(null);
    return sc;
}

char kbd_getchar(void) {
    char c = 0;
    while (!c) {
        scancode_t sc = kbd_wait_scan(false);
        c = translate_from_scan(NULL, sc);
    }
    return c;
}

/************* Keyboard driver ******************/

volatile bool theKeyboard[KEY_COUNT];

volatile kbd_event_f on_press = NULL;
volatile kbd_event_f on_release = NULL;

inline bool kbd_state_shift(void) {
    return theKeyboard[0x2A] | theKeyboard[0x36];
}

inline bool kbd_state_ctrl(void) {
    return theKeyboard[CTRL_MOD];
}

inline bool kbd_state(uint8_t scan_id) {
    return theKeyboard[scan_id];
}


void kbd_set_onpress(kbd_event_f onpress) {
    on_press = onpress;
}

void kbd_set_onrelease(kbd_event_f onrelease) {
    on_release = onrelease;
}


void keyboard_irq(/*void *stack*/) {
    uint8_t scan_code = 0;
    inb(0x60, scan_code);
    kbd_push_scancode(scan_code);

    logmsgdf("keyboard_irq(0x%x)\n", scan_code);

    if (!(scan_code & 0x80)) {
        /* on press event */
        theKeyboard[scan_code] = true;

        if (on_press)
            on_press(scan_code);
    } else {
        /* on release event */
        theKeyboard[scan_code & 0x7F] = false;

        if (on_release)
            on_release(scan_code);
    }
}

void kbd_setup(void) {
    memset((bool *)theKeyboard, 0, sizeof(theKeyboard));
    kbd_buf_setup();
    irq_set_handler(PS2_IRQ, keyboard_irq);
    irq_enable(PS2_IRQ);
}
