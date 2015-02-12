#include <log.h>
#include <arch/i386.h>

#define IDE_PRIMARY         0x1F0
#define IDE_PRIMARY_CTRL    0x3F6
#define IDE_SLAVE           0x170
#define IDE_SLAVE_CTRL      0x376

// port offsets
#define SECTCOUNT       0x2
#define LBA_lo          0x3
#define LBA_mid         0x4
#define LBA_hi          0x5
#define DRIVE_SELECT    0x6
#define CMDPORT         0x7
#define STATUSPORT      0x7

#define DRIVE_MASTER    0xA0
#define DRIVE_SLAVE     0xB0

#define IDE_CMD_IDENTIFY    0xEC

uint8_t ide_info_buf[512];

uint8_t ide_identify(uint port, uint8_t drive) {
    outb_p(port + DRIVE_SELECT, drive);   // identify Primary Master

    outb(port + SECTCOUNT, 0);
    outb(port + LBA_lo, 0);
    outb(port + LBA_mid, 0);
    outb(port + LBA_hi, 0);

    outb_p(port + CMDPORT, IDE_CMD_IDENTIFY);

    uint8_t status;
    inb(port + STATUSPORT, status);
    if (!status) return 0;

    while (status & 0x80)   inb_p(port + STATUSPORT, status);

    while (!(status & 0x09)) inb_p(port + STATUSPORT, status);

    if (status & 0x01) {
        logmsge("ide_identify: ERR bit set");
        return 0;
    }

    uint16_t p = port;
    n_insw(p, ide_info_buf, (uint16_t)256);

    return status;
}

void ide_init(uint prm_port, uint prm_ctrl, uint slv_port, uint slv_ctrl) {
    uint8_t ret = 0;
    ret = ide_identify(prm_port, DRIVE_MASTER);
    printf("Primary master identification: %x\n", (uint)ret);
}

void ide_setup(void) {
    /* assume they have fixed ports, not configuring through PCI */
    ide_init(IDE_PRIMARY, IDE_PRIMARY_CTRL, IDE_SLAVE, IDE_SLAVE_CTRL);
}
