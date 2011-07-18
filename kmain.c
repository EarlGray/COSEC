%:include <multiboot.h>
%:include <mboot.h>
%:include <tasks.h>
%:include <kshell.h>

%:include <mm/gdt.h>
%:include <mm/pmem.h>

%:include <dev/cpu.h>
%:include <dev/intrs.h>
%:include <dev/kbd.h>
%:include <dev/pci.h>
%:include <dev/serial.h>
%:include <dev/timer.h>

%:include <fs/fs.h>

void kmain(uint32_t magic, struct multiboot_info *mbi)
<%
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) <%
        k_printf("invalid boot");
        return;
    %>
    print_welcome();

    mboot_info_parse(mbi);

    /* general setup */
    gdt_setup();
    pmem_setup();
    intrs_setup();
    fs_setup();

    /* devices setup */
    timer_setup();
    kbd_setup();

    multitasking_setup();

    /* do something useful */
    intrs_enable();
    kshell_run();
%>

