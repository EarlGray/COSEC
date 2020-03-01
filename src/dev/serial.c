#include <dev/serial.h>
#include <dev/intrs.h>
#include <arch/i386.h>

#include <stdlib.h>

#include <cosec/log.h>

#define IER_OFFSET      1   /* Interrupt Enable Register */
#define IIFCR_OFFSET    2   /* Interrupt identification and FIFO control register */
#define LCR_OFFSET      3   /* Line Control Register */
#define MCR_OFFSET      4   /* Modem Control Register */
#define LSR_OFFSET      5   /* Line Status Register */
#define MSR_OFFSET      6   /* Nodem Status Register */
#define SCRATCH_REG     7   /* Scratch register */

volatile serial_on_receive_f on_receive = null;

inline void serial_set_on_receive(serial_on_receive_f handler) {
    on_receive = handler;
}

void serial_irq() {
    uint8_t iir;
    inb(COM1_PORT + IIFCR_OFFSET, iir);
    //logmsgf("IRQ4: IIR=%x\n", (uint)iir);

    if (serial_is_received(COM1_PORT)) {
        uint8_t b = serial_read(COM1_PORT);
        if (on_receive)
            on_receive(b);
    } 
}

inline void out_bits_b(uint16_t port, uint8_t mask, uint8_t val) {
    uint8_t port_val = 0;
    inb(port, port_val);
    port_val &= ~mask;
    port_val |= val;
    outb(port, port_val);
}

inline void out_bits_w(uint16_t port, uint16_t mask, uint16_t val) {
    uint16_t port_val = 0;
    inw(port, port_val);
    port_val &= ~mask;
    port_val |= val;
    outw(port, port_val);
}

inline void out_bits_l(uint16_t port, uint32_t mask, uint32_t val) {
    uint32_t port_val = 0;
    inl(port, port_val);
    port_val &= ~mask;
    port_val |= val;
    outl(port, port_val);
}

/* receives 1 for 115200 ticks per second, 2 for 57600 baud and so on */
inline void set_serial_divisor(uint16_t port, uint16_t div) {
    out_bits_b(port + LCR_OFFSET, 0x80, 0x80);
    logmsgf("0x%x port set to 0x%x\n", (uint)port, (uint)div);
    inb(port, div);
    out_bits_b(port + LCR_OFFSET, 0x80, 0x00);
}

/* receives 5..8 bit per character */
inline void set_serial_bitness(uint16_t port, uint8_t bit_per_char) {
    out_bits_b(port + LCR_OFFSET, 0x03, bit_per_char - 5);
}

inline bool serial_is_received(uint16_t port) {
    uint8_t val;
    inb(port + LSR_OFFSET, val);
    return val & 1;
}

inline uint8_t serial_read(uint16_t port) {
    uint8_t val;
    inb(port, val);
    return val;
}

inline bool serial_is_transmit_empty(uint16_t port) {
    uint8_t val;
    inb(port + LSR_OFFSET, val);
    return val & 0x20;
}

inline void serial_write(uint16_t port, uint8_t b) {
    outb_p(port, b);
}

void serial_puts(uint16_t port, const char *s) {
    //while (!serial_is_transmit_empty(COM1_PORT));
    while (*s) {
        switch (*s) {
          case '\n': serial_write(port, '\r'); /* fallthrough */
          default: serial_write(port, *s);
        }
        ++s;
    }
}

void serial_configure(uint16_t port, uint8_t speed_divisor) {
    outb(port + IER_OFFSET, 0);
    set_serial_divisor(port, speed_divisor);
    set_serial_bitness(port, 8);     /* 8bit byte */
    outb(port + IIFCR_OFFSET, 0xC7);    
    outb(port + MCR_OFFSET, 0x0B);   /* IRQ enabled, RTS/DSR set */
    outb(port + IER_OFFSET, 0x0F);   /* enable FIFO, clear, 14b threshold */
}

void serial_setup() {
    /* configure serial interface */
    serial_configure(COM1_PORT, 1);

    irq_set_handler(COM1_IRQ, serial_irq);
    irq_enable(COM1_IRQ);
}
