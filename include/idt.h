#ifndef __IDT_H
#define __IDT_H

#define IDT_SIZE            0x100

enum gatetype {
    GATE_INTR,      // Intel: interrupt gate, dpl = 0   Linux: intrgate
    GATE_TRAP,      // Intel: trap gate,    dpl = 0     Linux: trapgate
    GATE_CALL,      // Intel: trap gate,    dpl = 3     Linux: sysgate
    // no hardware multitasking
};

void idt_setup(void);
void idt_deploy(void);

#endif //__IDT_H
