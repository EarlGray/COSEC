#include <multiboot.h>
#include <kshell.h>

#include <arch/i386.h>
#include <mm/pmem.h>

#include <dev/kbd.h>
#include <dev/timer.h>
#include <dev/screen.h>

#include <tasks.h>
#include <mboot.h>

static void print_welcome(void) {
    clear_screen();

    int i;
    for (i = 0; i < 18; ++i) k_printf("\n");

    k_printf("\t\t\t<<<<< Welcome to COSEC >>>>> \n\n");
}

void kmain(uint32_t magic, struct multiboot_info *mbi) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        k_printf("invalid boot");
        return;
    }

    print_welcome();
    mboot_info_parse(mbi);

    /* general setup */
    cpu_setup();
    pmem_setup();
    multitasking_setup();

    /* devices setup */
    timer_setup();
    kbd_setup();

    /* do something useful */
    intrs_enable();
    kshell_run();
}

