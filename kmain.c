#include <multiboot.h>
#include <asm.h>
#include <gdt.h>
#include <physmem.h>
#include <intrs.h>

#include <kbd.h>
#include <timer.h>

#include <console.h>

#include <mboot.h>

void kmain(uint32_t magic, struct multiboot_info *mbi)
{
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        k_printf("invalid boot");
        return;
    }
    print_welcome();

    mboot_info_parse(mbi);

    /* general setup */
    gdt_setup();
    phmem_setup();
    intrs_setup();

    /* devices setup */
    timer_setup();
    kbd_setup();

    intrs_enable();

    /* do something useful */
    console_run();
}

