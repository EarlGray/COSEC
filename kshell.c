#include <kshell.h>

#include <mboot.h>
#include <multiboot.h>
#include <misc/test.h>

#include <mm/gdt.h>
#include <mm/pmem.h>

#include <dev/cpu.h>
#include <dev/screen.h>

#include <std/string.h>
#include <std/stdio.h>

/***
  *     Panic and other print routines
 ***/

void print_cpu(void);

void panic(const char *fatal_error) {
    intrs_disable();

    char buf [100] = { 0 };

    set_cursor_attr(0x4E);
    clear_screen();
    print_centered("O_o");
    print_centered("Oops, kernel panic");
    k_printf("\n");

    snprintf(buf, 100, "----> %s <-----", fatal_error);
    print_centered(buf);

    thread_hang();
}

void print_mem(char *p, size_t count) {
    char buf[100] = { 0 };
    char s = 0;

    int rest = (uint)p % 0x10;
    if (rest) {
        int indent = 10 + 3 * rest + (rest >> 2) + (rest % 4? 1 : 0);
        while (indent-- > 0) k_printf(" ");
    }

    size_t i;
    for (i = 0; i < count; ++i) {

        if (0 == (uint32_t)(p + i) % 0x10) {
            /* end of line */
            k_printf("%s\n", buf);

            /* start next line */
            s = snprintf(buf, 100, "%0.8x: ", (uint32_t)(p + i));
        }
    
        if (0 == (uint)(p + i) % 0x4) {
            s += snprintf(buf + s, 100 - s, " ");
        }

        int t = (uint8_t) p[i];
        s += snprintf(buf + s, 100 - s, "%0.2x ", t);
    }
    k_printf("%s\n", buf);
}

void print_cpu(void) {
    char buf[100];
    asm (" movl %0, %%eax \n" : : "r"(0xA5A5A5A5));
    asm (" movl %0, %%ebx \n" : : "r"(0xBBBBBBBB));
    cpu_snapshot(buf);
    print_mem(buf, 100);    
}

void print_welcome(void) {
    //set_cursor_attr(0x80);    // may cause errors on hardware
    clear_screen();

    int i;
    for (i = 0; i < 18; ++i) k_printf("\n");

    k_printf("\t\t\t<<<<< Welcome to COSEC >>>>> \n\n");
}


/***
  *     Console routines
 ***/

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



/***
  *     Temporary kernel shell
  *    @TODO: must be moved to userspace as soon as possible
 ***/

struct kshell_command {
    const char * name;
    void (*worker)(struct kshell_command *this, const char *arg);
    const char * description;
    const char * options;
};

void kshell_help();
void kshell_info(struct kshell_command *, const char *);
void kshell_mboot(struct kshell_command *, const char *);
void kshell_test(struct kshell_command *, const char *);
void kshell_mem(struct kshell_command *, const char *);
void kshell_panic();

struct kshell_command main_commands[] = {
    {   .name = "info",     .worker = kshell_info,  .description = "various info", .options = "stack gdt pmem colors cpu" },
    {   .name = "test",     .worker = kshell_test,  .description = "test utility", .options = "sprintf timer" },
    {   .name = "mem",      .worker = kshell_mem,   .description = "mem <start_addr> <size = 0x100>" },
    {   .name = "panic",    .worker = kshell_panic, .description = "test The Red Screen of Death"     },
    {   .name = "help",     .worker = kshell_help,  .description = "show this help"   },
    {   .name = null,       .worker = 0    }
};

void kshell_info(struct kshell_command *this, const char *arg) {
    if (!strcmp(arg, "stack")) {
        /* print stack addr */
        uint32_t stack;
        asm(" movl %%esp, %0 \n" : "=r"(stack));
        k_printf("stack at 0x%x\n", stack);
    } else
    if (!strcmp(arg, "gdt")) {
        gdt_info();
    } else
    if (!strcmp(arg, "pmem")) {
        pmem_info();
    } else
    if (!strcmp(arg, "colors")) {
        k_printf("Colors:\n");
        int i = 0;
        for (i = 0; i < 16; ++i) {
            char attr = get_cursor_attr();
            k_printf("%x) ", i);
            set_cursor_attr(i); k_printf("aaaa ");
            set_cursor_attr((uint8_t)(i << 4)); k_printf("bbbb");
            set_cursor_attr(attr); k_printf("\n");
        }
        k_printf("\n");
    } else
    if (!strcmp(arg, "cpu")) {
        print_cpu();
    } else
    {
        k_printf("Options: %s\n\n", this->options);
    }
}

void kshell_mem(struct kshell_command *this, const char *arg) {
    uint addr, size;
    arg = sscan_int(arg, (int *)&addr, 16);
    if (addr == 0) {
        k_printf("%s warning: reading 0x0000, default\n", this->name);
    }
    
    do {
        const char *end = sscan_int(arg, (int *)&size, 16);
        if (end != arg) break;
    } while (*(arg++));

    if (size == 0) 
        size = 0x100;

    print_mem((void *)addr, size);
    k_printf("\n");
}

void kshell_test(struct kshell_command *this, const char *cmdline) {
    if (!strncmp(cmdline, "sprintf", 4)) 
        test_sprintf();
    else if (!strncmp(cmdline, "timer", 5)) 
        test_timer();
    else {
        k_printf("options are %s\n\n", this->options);
    }
}


void kshell_unknown_cmd() {
    k_printf("type 'help'\n\n");
}

void kshell_panic() {
    panic("THIS IS JUST A TEST");
}

void kshell_help() {
    k_printf("Available commands:\n");
    struct kshell_command *cmd = main_commands;
    while (cmd->name) {
        k_printf("\t%s - %s\n", cmd->name, cmd->description);
        ++cmd;
    }
    k_printf("Available shortcuts:\n\tCtrl-L - clear screen\n\tEsc - on/off keyboard debug mode\n\n");
}


static char* get_args(char *command) {
    // find the first gap
    char *arg = command;
    while (*arg) {
        if (*arg == ' ') {
            while (*arg == ' ') 
                *(arg++) = 0;
            break;
        }
        ++arg;
    }
    return arg;
}

void kshell_do(char *command) {
    if (0 == *command) return;

    struct kshell_command *cmd = main_commands;
    char *arg = get_args(command);
    while (cmd->name) {
        if (!strcmp(cmd->name, command)) {
            cmd->worker(cmd, arg);
            return;
        }
        ++cmd;
    }

    kshell_unknown_cmd(arg);
}

#define ever (;;)
#define CMD_SIZE    256

void kshell_run(void) {
    char cmd_buf[CMD_SIZE] = { 0 };
      
    console_setup();
    
    for ever {
        console_write(prompt);
        console_readline(cmd_buf, CMD_SIZE);
        kshell_do(cmd_buf);
    }
}
