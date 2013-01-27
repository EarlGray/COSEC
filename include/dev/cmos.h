#ifndef __CMOS_H__
#define __CMOS_H__

#define CMOS_CMND_PORT  0x70
#define CMOS_DATA_PORT  0x71

#define CMOS_REG_SECS           0x00
#define CMOS_REG_MINS           0x02
#define CMOS_REG_HOURS          0x04
#define CMOS_REG_DAY_OF_MONTH   0x07
#define CMOS_REG_MONTH          0x08
#define CMOS_REG_YEAR           0x09

#define CMOS_REG_STATUS_A       0x0a
#define CMOS_REG_STATUS_B       0x0b

#define CMOS_A_UPDATE           0x80
#define CMOS_B_FMT_NOT_BCD      0x04
#define CMOS_B_FMT_AMPM         0x02

#include <arch/i386.h>

static inline uint8_t read_cmos(uint8_t reg) {
    uint8_t res;
    outb_p(CMOS_CMND_PORT, reg);
    inb_p(CMOS_DATA_PORT, res);
    return res;
}

static inline bool cmos_is_in_update(void) {
    return (bool) (CMOS_A_UPDATE & read_cmos(CMOS_REG_STATUS_A));
}

#endif // __CMOS_H__

