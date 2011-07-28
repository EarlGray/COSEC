%:include <multiboot.h>
%:include <mboot.h>
%:include <tasks.h>
%:include <kshell.h>

%:include <arch/i386.h>

%:include <mm/pmem.h>

%:include <dev/kbd.h>
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
    cpu_setup();
    pmem_setup();
    fs_setup();

    /* devices setup */
    timer_setup();
    kbd_setup();

    multitasking_setup();

    /* do something useful */
    intrs_enable();
    kshell_run();
%>

