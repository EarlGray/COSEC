#ifndef __SERIAL_H__
#define __SERIAL_H__

#define COM1_PORT   0x03F8
#define COM2_PORT   0x02F8
#define COM3_PORT   0x03E8 
#define COM4_PORT   0x02E8

typedef void (*serial_on_receive_f)(uint8_t b);
    
bool serial_is_received(uint16_t port);
uint8_t serial_read(uint16_t port);
bool serial_is_transmit_empty(uint16_t port);
void serial_write(uint16_t port, uint8_t b);

void serial_set_on_receive(serial_on_receive_f handler);

void serial_setup(void);

#endif // __SERIAL_H__
