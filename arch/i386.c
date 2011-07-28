#include <arch/i386.h>

#include <arch/idt.h>
#include <arch/gdt.h>
#include <arch/intr.h>

#include <dev/intrs.h>


/***
  *     Internal declarations
 ***/

#define N_TASKS     40
#define N_GDT       (5 + N_TASKS * 2)

/* GDT indeces */
#define GDT_DUMMY       0
#define GDT_KERN_CS     1
#define GDT_KERN_DS     2
#define GDT_USER_CS     3
#define GDT_USER_DS     4

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void gdt_get(struct gdt_ptr *idt);

#define SEL_KERN_CS     ((GDT_KERN_CS << 3) + (PL_KERN << 1))
#define SEL_KERN_DS     ((GDT_KERN_DS << 3) + (PL_KERN << 1))
#define SEL_USER_CS     ((GDT_USER_CS << 3) + (PL_USER << 1))
#define SEL_USER_DS     ((GDT_USER_DS << 3) + (PL_USER << 1))



#define IDT_SIZE            0x100


/*****************************************************************************
        GDT
******************************************************************************/

#if (N_GDT > 8191)
    #error "N_TASKS is too big"
#endif

segment_descriptor theGDT[N_GDT];

#define gdt_entry_init(index, type, pl)     segdescr_usual_init(theGDT[index], type, 0xFFFFF, 0, pl, SD_GRAN_4Kb)

extern void gdt_load(uint16_t limit, void *base);

void gdt_setup(void) {
    memset(theGDT, 0, N_GDT * sizeof(struct segdescr));

    gdt_entry_init((SEL_KERN_CS >> 3), SD_TYPE_ER_CODE, PL_KERN);
    gdt_entry_init((SEL_KERN_DS >> 3), SD_TYPE_RW_DATA, PL_KERN);
    gdt_entry_init((SEL_USER_CS >> 3), SD_TYPE_ER_CODE, PL_USER);
    gdt_entry_init((SEL_USER_DS >> 3), SD_TYPE_RW_DATA, PL_USER);

    gdt_load(N_GDT, theGDT);
}

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

inline segment_descriptor * i386_gdt(void) {
    return theGDT;
}

inline segment_descriptor * i386_idt(void) {
    return theIDT;
}


void cpu_setup(void) {
    gdt_setup();

    intrs_setup();

    idt_setup();
    idt_deploy();
}
