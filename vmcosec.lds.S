#undef i386

OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
SECTIONS
{
    . = KERN_PA + KERN_OFF;
    _entry = .;

    _text = .;
    text = .;
    .text : 
    AT(ADDR(.text) - KERN_OFF)
    {
        *(.text .stub .text.*)
        *(.rodata .rodata.* )
    }
    _etext =  .;
    etext =   .;

    _data = .;
    .data :
    AT(ADDR(.data) - KERN_OFF)
    ALIGN(0x10)
    {
        *(.data .data.* )
    }
    .bss :
    AT(ADDR(.bss) - KERN_OFF)
    ALIGN(0x10)
    {
        *(.bss .bss.* )
        *(COMMON)
    }
    _end = .;
    end =  .;
}
