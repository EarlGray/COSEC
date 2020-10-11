#ifndef __INTR_ASM_H
#define __INTR_ASM_H

// sizeof(struct interrupt_context)
#define CONTEXT_SIZE        0x30

#ifndef NOTCC

#include "arch/i386.h"

struct interrupt_context {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
};

extern uint32_t  intr_err_code(void);
extern void* intr_context_esp(void);
extern void  intr_set_context_esp(uintptr_t esp);

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


extern void irq00(void);
extern void irq01(void);
extern void irq02(void);
extern void irq03(void);
extern void irq04(void);
extern void irq05(void);
extern void irq06(void);
extern void irq07(void);
extern void irq08(void);
extern void irq09(void);
extern void irq0A(void);
extern void irq0B(void);
extern void irq0C(void);
extern void irq0D(void);
extern void irq0E(void);
extern void irq0F(void);

extern void syscallentry(void);     // for system call interrupt
extern void dummyentry(void);       // for all unused software interrupts
extern void isr14to1F(void);        // reserved

typedef void (*intr_entry_f)(void);

#endif
#endif // __INTR_ASM_H
