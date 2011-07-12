/* 
 *     This file represents IDT as a single object and incapsulates hardware 
 *  related functions, interfaces with assembly part of code.
 *      
 *      About IDT
 *  IDT mapping is quite linux'ish, for more details see idt_setup() below
 *  User system call defined for the future, its number defined at idt.h as
 *  SYS_INT (now it's 0x80 like Linux)
 *    There are 3 types of IDT entries (like Linux): trap gates (traps/faults 
 *  with dpl=0,  linux's trap gates),  call gates (traps/faults with dpl=3, 
 *  linux's system gates), intr gates (interrupt gates, linux's interrupts)
 */

#include <dev/idt.h>

#include <mm/gdt.h>
#include <dev/cpu.h>
#include <dev/intr.h>
#include <dev/intrs.h>

/*****  The IDT   *****/
segment_descriptor  theIDT[IDT_SIZE];

#define idt_set_gate(index, type, addr)                                 \
    segdescr_gate_init(theIDT[index], SEL_KERN_CS, (uint32_t)(addr),    \
            ((type) == GATE_CALL ? PL_USER : PL_KERN),                  \
            ((type) == GATE_TRAP ? 1 : 0))

inline void idt_set_gates(uint8_t start, uint16_t end, enum gatetype type, intr_entry_f intr_entry) {
    int i;
    for (i = start; i < end; ++i) 
        idt_set_gate(i, type, intr_entry);
}

void idt_setup(void) {
    int i;
    /* 0x00 - 0x1F : exceptions entry points */
    for (i = 0; i < 0x14; ++i)
        idt_set_gate(i, exceptions[i].type, exceptions[i].entry);
    idt_set_gates(0x14, 0x20, GATE_INTR, isr14to1F);

    /* 0x20 - 0xFF : dummy software interrupts */
    idt_set_gates(0x20, 0x100, GATE_CALL, dummyentry);

    /* 0x20 - 0x2F : IRQs entries */
    for (i = I8259A_BASE; i < I8259A_BASE + 16; ++i)
        idt_set_gate(i, GATE_INTR, interrupts[i - I8259A_BASE]);

    /* 0xSYS_INT : system call entry */
    idt_set_gate(SYS_INT, GATE_CALL, syscallentry);
}


extern void idt_load(uint16_t limit, uint32_t addr);

void idt_deploy(void) {
    idt_load(IDT_SIZE * sizeof(segment_descriptor) - 1, (uint32_t) theIDT);
}

