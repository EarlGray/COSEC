#include <console.h>
#include <stdio.h>
#include <defs.h>

#define ever (;;)

char *prompt = ">> ";

inline void console_write(const char *msg) {
    k_printf("%s", msg);
}

inline void console_writeln(const char *msg) {
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
        default:
            if (cur - buf + 1 < size) {
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
        console_writeln(cmd_buf);
    }
}
