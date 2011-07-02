#include <console.h>
#include <std/stdio.h>
#include <dev/screen.h>
#include <kshell.h>

void print_welcome()
{
    //set_cursor_attr(0x80);    // may cause errors on hardware
    clear_screen();

    int i;
    for (i = 0; i < 18; ++i) k_printf("\n");

    k_printf("\t\t\t<<<<< Welcome to COSEC >>>>> \n\n");
}

#define ever (;;)

char *prompt = ">> ";

inline void console_write(const char *msg) {
    k_printf("%s", msg);
}

inline void console_writeline(const char *msg) {
    k_printf("%s\n", msg);
}

void console_readline(char *buf, size_t size) {
    char *cur = buf;
    while (1) {
        char c = getchar();
           
        switch (c) {
        case '\n': 
            cur[0] = 0;
            k_printf("\n");
            return;
        case '\b':
            if (cur == buf) break;
            k_printf("\b \b");
            --cur;
            break;
        case 12:    // Ctrl-L
            *cur = 0;
            clear_screen();
            console_write(prompt);
            console_write(buf);
            break;
        default:
            if (cur - buf + 1 < (int)size) {
                *cur = c;
                ++cur;
                putchar(c);
            }
        }
    }
}

static void console_setup(void) {
    //
}

#define CMD_SIZE    256

void console_run(void) {
    char cmd_buf[CMD_SIZE] = { 0 };
      
    console_setup();
    
    for ever {
        console_write(prompt);
        console_readline(cmd_buf, CMD_SIZE);
        kshell_do(cmd_buf);
    }
}
