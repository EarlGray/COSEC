#include <multiboot.h>
#include <mboot.h>
#include <asm.h>

#include <mm/gdt.h>
#include <mm/physmem.h>

#include <dev/intrs.h>
#include <dev/kbd.h>
#include <dev/timer.h>

#include <console.h>


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

