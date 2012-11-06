#include <arch/i386.h>
#include <arch/mboot.h>

#include <dev/devices.h>
#include <dev/kbd.h>
#include <dev/timer.h>
#include <dev/screen.h>

#include <mem/virtmem.h>
#include <fs/vfs.h>

#include <kshell.h>
#include <tasks.h>

static void print_welcome(void) {
    clear_screen();

    int i;
    for (i = 0; i < 18; ++i) k_printf("\n");

    k_printf("\t\t\t<<<<< Welcome to COSEC >>>>>\n\n");
}

void kinit(uint32_t magic, struct multiboot_info *mbi) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        k_printf("invalid boot\n\n");
        return;
    }

    print_welcome();
    mboot_info_parse(mbi);

    /* general setup */
    cpu_setup();
    memory_setup();

    /* devices setup */
    dev_setup();
    timer_setup();
    kbd_setup();

    /* do something useful */
    tasks_setup();

    vfs_setup();

    intrs_enable();
    kshell_run();
}

