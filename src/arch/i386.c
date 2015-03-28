#include <arch/i386.h>
#include <arch/intr.h>

#include <dev/intrs.h>

#include <string.h>

/***
  *     Internal declarations
 ***/

#define N_TASKS     40
#define N_GDT       (5 + N_TASKS * 2)

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void gdt_get(struct gdt_ptr *idt);

#define IDT_SIZE            0x100


/*****************************************************************************
        routines
******************************************************************************/
ptr_t cpu_stack(void) {
    ptr_t esp;
    i386_esp(esp);
    return esp + 8;   // this function ret addr (4) + C push %ebp (4)
}

inline uint x86_eflags(void) {
    uint efl;
    i386_eflags(efl);
    return efl;
}

/*****************************************************************************
        GDT
******************************************************************************/

#if (N_GDT > 8191)
    #error "N_TASKS is too big"
#endif

segment_descriptor theGDT[N_GDT];

#define gdt_entry_init(index, type, pl)     segdescr_usual_init(theGDT[index], type, 0xFFFFF, 0, pl, SD_GRAN_4Kb)

const segment_descriptor defLDT[] = {
    {   .as.ll = 0x00CFFA000000FFFFull  },
    {   .as.ll = 0x00CFF2000000FFFFull  },
};

extern void gdt_load(uint16_t limit, void *base);

void gdt_setup(void) {
    memset(theGDT, 0, N_GDT * sizeof(struct segdescr));

    gdt_entry_init(GDT_KERN_CS, SD_TYPE_ER_CODE, PL_KERN);
    gdt_entry_init(GDT_KERN_DS, SD_TYPE_RW_DATA, PL_KERN);
    gdt_entry_init(GDT_USER_CS, SD_TYPE_ER_CODE, PL_USER);
    gdt_entry_init(GDT_USER_DS, SD_TYPE_RW_DATA, PL_USER);
    segdescr_usual_init(theGDT[GDT_DEF_LDT], SD_TYPE_LDT, 
            8 * 2/*sizeof(defLDT)/sizeof(segment_descriptor)*/, (uint)defLDT, 
            PL_USER, SD_GRAN_4Kb);

    gdt_load(N_GDT, theGDT);
}

index_t gdt_alloc_entry(segment_descriptor entry) {
    index_t i;
    for (i = 1; i < N_GDT; ++i) 
        if (0 == *(uint *)(theGDT + i)) {
            theGDT[i] = entry;
            return i;
        }
    return 0;
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

void print_eflags(struct eflags efl) {
    //logmsgif("cf = 
}

/***
  *     Task-related routines
 ***/ 


void cpu_setup(void) {
    gdt_setup();

    intrs_setup();

    idt_setup();
    idt_deploy();
}
