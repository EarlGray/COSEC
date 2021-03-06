#include <arch/i386.h>
#include <arch/mboot.h>

#include <dev/kbd.h>
#include <dev/timer.h>
#include <dev/screen.h>
#include <dev/pci.h>

#include <mem/pmem.h>
#include <fs/devices.h>
#include <fs/vfs.h>

#include <kshell.h>
#include <tasks.h>
#include <process.h>

#include <cosec/log.h>

static void print_welcome(void) {
    vcsa_clear(CONSOLE_VCSA);

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

    logging_setup();

    /* hardware setup */
    timer_setup();
    kbd_setup();

    /* do something useful */
    dev_setup();
    vfs_setup();

    intrs_enable();
    pci_setup();

#ifdef COSEC_RUST
    hello_rust();
#endif
    proc_setup();
    panic("kinit: an unreachable point");
}
