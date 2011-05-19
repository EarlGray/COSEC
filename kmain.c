#include <asm.h>
#include <multiboot.h>
#include <kconsole.h>
#include <memory.h>
#include <intrs.h>

void print_welcome()
{
    set_cursor_attr(0x80);
    clear_screen();

    int i;
    for (i = 0; i < 18; ++i) k_printf("\n");

    k_printf("\t\t\t\t   O_o\n");
    k_printf("\t\t\t<<<<< Welcome to COSEC >>>>> \n\n");
}

void print_mem(char *p, size_t count) {
    int i;
    for (i = 0; i < count; ++i) {
        if (0 == (uint32_t)(p + i) % 0x10) 
            k_printf("\n%x : ", (uint32_t)(p + i));
        int t = (uint8_t) p[i];
        k_printf("%x ", t);
    }
    k_printf("\n");
}

void kmain(uint32_t magic, uint32_t mbi_addr)
{
    multiboot_info_t *mbi = (multiboot_info_t *) mbi_addr;
        
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        print_at(0, 10, "invalid boot");
        return;
    }

    print_welcome();

    memory_setup();
    intrs_setup();

    k_printf("\nGDT:");
    print_mem((char *)0x116880, 0x20);
    k_printf("\nIDT:");
    print_mem((char *)0x116040, 0x40);

    intrs_enable();

    uint32_t stack;
    asm(" movl %%esp, %0 \n" : "=r"(stack)::);
    k_printf("stack at 0x%x\n", stack);

    //asm ("into \n");  //*/
    thread_hang();
}

