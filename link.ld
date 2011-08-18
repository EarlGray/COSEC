OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
/*STARTUP(./build/arch/boot.o)*/
SECTIONS
{
    VIRT_SHIFT = 0x00000000;
    LOAD_ADDR = 0x100000;
    . = LOAD_ADDR + VIRT_SHIFT;
    _entry = .;

    _text = .;     text = .;
    .text : 
    AT(ADDR(.text) - VIRT_SHIFT) 
    {
        *(.text .stub .text.*)
        *(.rodata .rodata.* )  
    }
    _etext =  .;   etext =   .;

    _data = .;
    .data :
    AT(ADDR(.data) - VIRT_SHIFT)
    ALIGN(0x10)
    {
        *(.data .data.* ) 
    }
    .bss :
    AT(ADDR(.bss) - VIRT_SHIFT)
    ALIGN(0x10)
    { 
        *(.bss .bss.* ) 
        *(COMMON) 
    }
    _end = .;   end =  .;
}
