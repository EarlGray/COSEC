#include <kshell.h>

#include <globl.h>
#include <mboot.h>
#include <multiboot.h>
#include <asm.h>

#include <mm/gdt.h>
#include <mm/pmem.h>
#include <dev/screen.h>

#include <std/string.h>

/***
  *     Panic
 ***/

void panic(const char *fatal_error) {
    intrs_disable();

    set_cursor_attr(0x40);
    clear_screen();
    k_printf("\n\n\t\t\t\t   O_o\n");
    k_printf("\n\t\t\t   Ooos, kernel panic");
    k_printf("\n\n\t%s\n", fatal_error);

    thread_hang();
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
void kshell_panic();

struct kshell_command main_commands[] = {
  //  {   .name = "mboot",    .worker = kshell_mboot, .description = "show boot info", .options = "mmap"  },
    {   .name = "info",     .worker = kshell_info,  .description = "various info", .options = "stack gdt pmem" },
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
    } else {
        k_printf("Options: %s\n\n", this->options);
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
