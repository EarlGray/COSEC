#include <screen.h>
#include <kbd.h>

volatile char c = 0;

static void on_key_press(uint8_t scan_code) {
    c = translate_from_scan(null, kbd_pop_scancode());
}

int getchar(void) {
    kbd_set_onpress(on_key_press);
    c = 0;
    while (c == 0);
    kbd_set_onpress(null);
    return c;
}

int putchar(int c) {
    putc_at(get_cursor_x(), get_cursor_y(), (uint8_t)c, get_cursor_attr());
    move_cursor_next();
}
