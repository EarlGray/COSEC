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

#include <idt.h>
#include <misc.h>
#include <memory.h>
#include <intr.h>
#include <intrs.h>

/* IDT entry structure */
struct gatedescr {
    uint16_t addr_l;
    struct segsel seg;
    uint8_t rsrvdb;     // always 0
    uint8_t trap_bit:1;
    uint8_t rsrvd32:4;  // always 0x3
    uint8_t dpl:2;
    uint8_t present:1;
    uint16_t addr_h;
};

/*****  The IDT   *****/
struct gatedescr  theIDT[IDT_SIZE];

inline void gatedescr_set(struct gatedescr *gated, enum gatetype type, struct segsel seg, uint32_t addr, enum privilege_level dpl) {
    gated->addr_l = (uint16_t) addr;
    gated->seg = seg;
    gated->dpl = dpl;
    gated->trap_bit = (type == GATE_TRAP ? 1 : 0);
    gated->rsrvdb = 0;
    gated->rsrvd32 = 0x7;
    gated->present = 1;
    gated->addr_h = (uint16_t) (addr >> 16);
}

inline void idt_set_gate(uint8_t i, enum gatetype type, intr_entry_f intr_entry) {
    switch (type) {
    case GATE_TRAP: 
        gatedescr_set(theIDT + i, GATE_TRAP, SEL_KERN_CS, (uint32_t)intr_entry, PL_KERN);
        break;
    case GATE_CALL:
        gatedescr_set(theIDT + i, GATE_CALL, SEL_KERN_CS, (uint32_t)intr_entry, PL_USER);
        break;
    case GATE_INTR:
        gatedescr_set(theIDT + i, GATE_INTR, SEL_KERN_CS, (uint32_t)intr_entry, PL_KERN);
        break;
    }
}

inline void idt_set_gates(uint8_t start, uint8_t end, enum gatetype type, intr_entry_f intr_entry) {
    int i;
    for (i = start; i < end; ++i) 
        idt_set_gate(i, type, intr_entry);
}

void idt_setup(void) {
    int i;
    /* 0x00 - 0x20 : exceptions entry points */
    for (i = 0; i < 0x14; ++i)
        idt_set_gate(i, exceptions[i].type, exceptions[i].entry);
    idt_set_gates(0x14, 0x20, GATE_INTR, isr14to1F);

    /* 0x20 - 0x2F : IRQs entries */
    for (i = I8259A_BASE_VECTOR; i < I8259A_BASE_VECTOR + 16; ++i)
        idt_set_gate(i, GATE_INTR, interrupts[i - I8259A_BASE_VECTOR]);

    /* 0x30 - 0xFF : dummy software interrupts */
    for (i = 0x30; i < 0x100; ++i)
        idt_set_gate(i, GATE_CALL, dummyentry);

    /* 0xSYS_INT : system call entry */
    idt_set_gate(SYS_INT, GATE_CALL, syscallentry);
}

extern void idt_load(uint16_t limit, uint32_t addr);

void idt_deploy(void) {
    idt_load(IDT_SIZE * sizeof(struct gatedescr) - 1, (uint32_t) theIDT);
}

