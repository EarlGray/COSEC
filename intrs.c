#include <misc.h>
#include <asm.h>

#include <intrs.h>

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

inline void
idt_load(void) {
	uint32_t idtr[3];

}

static void 
intrs_remap(uint8_t master, uint8_t slave) {
    uint8_t master_mask, slave_mask;

    // save masks
    inb_p(PIC1_DATA_PORT, master_mask);
    inb_p(PIC2_DATA_PORT, slave_mask);

    // ICWs
    outb_p(PIC1_CMD_PORT, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb_p(PIC2_CMD_PORT, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb_p(PIC1_DATA_PORT, master);
    io_wait();
    outb_p(PIC2_DATA_PORT, slave);
    io_wait();

    outb_p(PIC1_DATA_PORT, 4);
    io_wait();
    outb_p(PIC2_DATA_PORT, 2);
    io_wait();

    outb_p(PIC1_DATA_PORT, ICW4_I8086);
    io_wait();
    outb_p(PIC2_DATA_PORT, ICW4_I8086);
    io_wait();

    // restore mask
    outb_p(PIC1_DATA_PORT, master_mask);
    outb_p(PIC2_DATA_PORT, slave_mask);
}

void intrs_setup(void) {
	
}

void dummy_interrupt(void) {

}
