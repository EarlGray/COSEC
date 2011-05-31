#include <dev/screen.h>
#include <dev/kbd.h>
#include <asm.h>

volatile char c = 0;

static void on_key_press(uint8_t scan_code) {
    c = translate_from_scan(null, scan_code);
}

int getchar(void) {
    kbd_set_onpress(on_key_press);
    c = 0;
    while (c == 0) cpu_halt();
    kbd_set_onpress(null);
    return c;
}

int putchar(int c) {
    putc_at(get_cursor_x(), get_cursor_y(), (uint8_t)c, get_cursor_attr());
    move_cursor_next();
    return c;
}
