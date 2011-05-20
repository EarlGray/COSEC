#include <multiboot.h>
#include <asm.h>
#include <memory.h>
#include <intrs.h>

#include <kbd.h>
#include <timer.h>

#include <console.h>

void kmain(uint32_t magic, uint32_t mbi_addr)
{
    multiboot_info_t *mbi = (multiboot_info_t *) mbi_addr;
        
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        k_printf("invalid boot");
        return;
    }

    memory_setup();
    intrs_setup();

    timer_setup();
    kbd_setup();

    intrs_enable();

    uint32_t stack;
    asm(" movl %%esp, %0 \n" : "=r"(stack)::);
    k_printf("stack at 0x%x\n", stack);

    console_run();
    thread_hang();
}

