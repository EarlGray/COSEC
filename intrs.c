#include <misc.h>
#include <asm.h>

#include <intrs.h>
#include <memory.h>

#define ICW1_ICW4       0x01
#define ICW1_SINGLE     0x02        // 0x00 - cascade
#define ICW1_INTERVAL   0x04        // 0x00 - 
#define ICW1_LEVEL      0x08        // triggered (edged) mode
#define ICW1_INIT       0x10        // reqired

#define ICW4_I8086      0x01        // i8086 (i8080) mode
#define ICW4_AUTO       0x02        // auto (normal) EOI
#define ICW4_BUF_SLAVE  0x08        // buffered mode/slave
#define ICW4_BUF_MASTER 0x0C        // buffered mode/master
#define ICW4_SFNM       0x10        // special fully nested (not)

#define PIC1_CMD_PORT    0x20
#define PIC1_DATA_PORT   (PIC1_CMD_PORT + 1)
#define PIC2_CMD_PORT    0xA0
#define PIC2_DATA_PORT   (PIC2_CMD_PORT + 1)

#define PIC_EOI         0x20            // PIC end-of-interrput command code

#define I8259A_BASE_VECTOR  0x20
#define IDT_SIZE            0x100

/* 
 *      Declarations
 */

typedef enum {
    FLT_DE = 0, INT_DB,     INT_NMI,    TRP_BP,    
    TRP_OF,     FLT_BR,     FLT_UD,     FLT_NM,
    ABR_DF,     INT_FP,     FLT_TS,     FLT_NP,
    FLT_SS,     FLT_GP,     FLT_PF,     INT0x0F,
    FLT_MF,     FLT_AC,     ABR_MC,     FLT_XM,
} intr_name;

typedef enum {
    GATE_INTR = 0,
    GATE_TRAP,
    GATE_CALL,
    // GATE_TASK - no hardware multitasking
} gatetype_e;

typedef struct {
    uint16_t addr_l;
    segsel_t seg;
    uint8_t rsrvdb;
    uint8_t trap_bit:1;
    uint8_t rsrvd32:4;  // always 0x3
    uint8_t dpl:2;
    uint8_t present:1;
    uint16_t addr_h;
} gatedescr_t;

// enter points
extern void isr00(void);    // #DE, division by zero,   fault,  no code
extern void isr01(void);    // #DB, reserved
extern void isr02(void);    // NMI, Interrupt, no
extern void isr03(void);    // #BP. breakpoint,         trap,   no
extern void isr04(void);    // #OF, overflow,           trap,   no
extern void isr05(void);    // #BR, out of bounds,      fault,  no
extern void isr06(void);    // #UD, invalid opcode,     fault,  no
extern void isr07(void);    // #NM, no math coproc.,    fault,  no
extern void isr08(void);    // #DF, double fault,       abort,  0
extern void isr09(void);    // Invalid floating-point op (reserved)
extern void isr0A(void);    // #TS, invalid TSS.        fault,  yes

extern void isr0B(void);    // #NP, segment not present fault,  yes
extern void isr0C(void);    // #SS, stack-segment       fault.  yes
extern void isr0D(void);    // #GP, General protection  fault,  yes
extern void isr0E(void);    // #PF. page                fault,  yes
extern void isr0F(void);    // reserved by intel
extern void isr10(void);    // #MF, math (FPU)          fault,  no
extern void isr11(void);    // #AC, alignment check     fault,  0
extern void isr12(void);    // #MC, machine check       abort,  no
extern void isr13(void);    // #XM, SIMD fp exc         fault,  no

extern void isr14to1F(void);    // reserved

#define init_idt_size   20
struct {
    gatetype_e type,
    void (*entry)(void),
    privilege_level_e priv,
} init_idt[init_idt_size] = {
    { GATE_TRAP, isr00, PL_KERN },
};
    
intr_handler_f intr_handler_table[IDT_SIZE];

void gatedescr_init(gatedescr_t *gated, segsel_t seg, uint32_t addr, privilege_level_e dpl);

/* 
 *    Implementations
 */

gatedescr_t theIDT[IDT_SIZE];

inline void
idt_load(void) {
    uint32_t idtr[3];
    idtr[0] = (IDT_SIZE << 16);
    idtr[1] = theIDT;

    lidt(((char *) idtr) + 2);
}

static void 
irq_remap(uint8_t master, uint8_t slave) {
    uint8_t master_mask, slave_mask;

    // save masks
    inb(PIC1_DATA_PORT, master_mask);
    inb(PIC2_DATA_PORT, slave_mask);

    // ICWs
    outb_p(PIC1_CMD_PORT, ICW1_INIT | ICW1_ICW4);
    outb_p(PIC2_CMD_PORT, ICW1_INIT | ICW1_ICW4);

    outb_p(PIC1_DATA_PORT, master);
    outb_p(PIC2_DATA_PORT, slave);

    outb_p(PIC1_DATA_PORT, 4);
    outb_p(PIC2_DATA_PORT, 2);

    outb_p(PIC1_DATA_PORT, ICW4_I8086);
    outb_p(PIC2_DATA_PORT, ICW4_I8086);

    // restore mask
    outb(PIC1_DATA_PORT, master_mask);
    outb(PIC2_DATA_PORT, slave_mask);
}

inline void gatedescr_init(gatedescr_t *gated, gatetype_e type, segsel_t seg, uint32_t addr, privilege_level_e dpl) {
    gated->addr_l = (uint16_t) addr;
    gated->seg = seg;
    gated->dpl = dpl;
    gated->trap_bit = (type == GATE_TRAP ? 1 : 0);
    gated->rsrvdb = 0;
    gated->rsrvd32 = 0x3;
    gated->present = 1;
    gated->addr_h = (uint16_t) (addr >> 16);
}

void intrs_setup(void) {
    //  remap interrupts   
    irq_remap(I8259A_BASE_VECTOR, I8259A_BASE_VECTOR + 8);

    int i;
    // prepare handlers
    for (i = 0; i < IDT_SIZE; ++i) {
        intr_handler_table[i] = dummy_interrupt;
    }

    //  prepare idt     
    for (i = 0; i < init_idt_size; ++i)
        gatedescr_init(theIDT + i, init_idt[i].type, KERN_CODE_SEG, init_idt[i].entry, init_idt[i].priv);

    for (i = init_idt_size; i < 0x20; ++i)
        gatedescr_init(theIDT + i, GATE_FAULT, KERN_CODE_SEG, isr14to1F, PL_KERN);

    for (i = I8259A_BASE_VECTOR; i < I8259A_BASE_VECTOR+8; ++i)
        gatedescr_init(theIDT + i, GATE_IRQ, ;
        

    idt_load();
}

void dummy_interrupt(int intrnum) {
    k_printf("interrupt 0x%x\n", intrnum);
}
