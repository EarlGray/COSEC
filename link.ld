OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
SECTIONS
{
    VIRT_SHIFT = 0x00000000;
    LOAD_ADDR = 0x100000;
    . = LOAD_ADDR + VIRT_SHIFT;
    PROVIDE (_entry = .);

    PROVIDE (_text =  .);
    PROVIDE (text =   .);
    .text : 
    AT(ADDR(.text) - VIRT_SHIFT) 
    {
        *(.text .stub .text.*)
        *(.rodata .rodata.* )  
    }
    PROVIDE (_etext =  .);
    PROVIDE (etext =   .);

    PROVIDE (_data =   .);
    .data :
    AT(ADDR(.data) - 0xc0000000)
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
    PROVIDE (_end = .);
    PROVIDE (end =  .);
}
