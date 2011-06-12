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
    cprint((uint8_t)c);
    return c;
}

/***
  *     I/O
 ***/


