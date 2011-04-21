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
    for (i = 0; i < 19; ++i) k_printf("\n");

	k_printf("\t\t\t<<<<< Welcome to COSEC >>>>> \n\n");
}

void kmain(uint32_t magic, uint32_t mbi_addr)
{
	multiboot_info_t *mbi = (multiboot_info_t *) mbi_addr;
		
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		print_at(0, 10, "invalid boot");
		return;
	}

	memory_setup();
    intrs_setup();

    print_welcome();
    k_printf("   We are here ->");

    /*/ test #DE
    int a = 0;
    int b = 10/a;*/

    thread_hang();
}

