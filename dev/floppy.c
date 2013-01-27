#include <arch/i386.h>
#include <dev/intrs.h>
#include <dev/cmos.h>

#define FDC0    0x3f0
#define FDC1    0x370

#define FDC_DOR     2   /* Digital Output Register */
#define FDC_MSR     4   /* Main Status Register */
#define FDC_DATA    5   /* Data Register */

/* DOR bits */
#define FDC_DOR_DR0     0
#define FDC_DOR_DR1     1
#define FDC_DOR_DR2     2
#define FDC_DOR_DR3     3
#define FDC_ENABLE      (1<<2)
#define FDC_DMA_MODE    (1<<3)
#define FDC_DOR_MOTOR0  (1<<4)
#define FDC_DOR_MOTOR1  (1<<5)
#define FDC_DOR_MOTOR2  (1<<6)
#define FDC_DOR_MOTOR3  (1<<7)

/* MSR bits */
#define FDC_MSR_BUSY_SEEK0   (1<<0)
#define FDC_MSR_BUSY_SEEK1   (1<<1)
#define FDC_MSR_BUSY_SEEK2   (1<<2)
#define FDC_MSR_BUSY_SEEK3   (1<<3)
#define FDC_MSR_BUSY_RW      (1<<4)
#define FDC_MSR_NONDMA_MODE     (1<<5)
#define FDC_MSR_FDC_HAS_DATA    (1<<6)
#define FDC_MSR_DATAREG_READY   (1<<7)

/* AT only */
#define FDC_AT_CCR  7   /* Conf. Control Reg. */
#define FDC_AT_DIR  7   /* Digital Input Reg */

/* PS/2 only */
#define FDC_PS2_SRA 0   /* Status Reg. A */
#define FDC_PS2_SRB 1   /* Status Reg. B */
#define FDC_PS2_DSR 4   /* Data Rate Select Reg */

#define dma_mask_channel(ch)
#define dma_unmask_channel(ch)
#define dma_reset_master_flipflop()
#define dma_set_addr(addr)

typedef enum {
    FDC_CMD_READ_TRACK  =   2,
    FDC_CMD_SPECIFY     =   3,
    FDC_CMD_CHECK_STAT  =   4,
    FDC_CMD_WRITE_SECT  =   5,
    FDC_CMD_READ_SECT   =   6,
    FDC_CMD_CALIBRATE   =   7,
    FDC_CMD_CHECK_INT   =   8,
    FDC_CMD_WRITE_DEL_S =   9,
    FDC_CMD_READ_ID_S   =   0xa,
    FDC_CMD_READ_DEL_S  =   0xc,
    FDC_CMD_FORMAT_TRACK =  0xd,
    FDC_CMD_SEEK        =   0xf
} floppy_cmd_e; 

void floppy_dma_init() {
    dma_mask_channel(2);
    dma_reset_master_flipflop();
    dma_set_addr(0x1000);
    
    dma_unmask_channel(2);
}

void floppy_dma_write() {
    dma_mask_channel(2);
    
    dma_unmask_channel(2);
}

void floppy_dma_read() {
    dma_mask_channel(2);
    dma_unmask_channel(2);
}

void floppy_cmd(floppy_cmd_e cmd) {
       
}

void floppy_irq_handler(void *stack) {
    k_printf("IRQ6\n");
}

static const char * flp_cmos_type[] = {
    [0] = "none",
    [1] = "360 KB 5.25 Drive",
    [2] = "1.2 MB 5.25 Drive",
    [3] = "720 KB 3.5 Drive",
    [4] = "1.44 MB 3.5 Drive",
    [5] = "2.88 MB 3.5 drive"
};

void floppy_setup(void) {
    uint8_t flpinfo = read_cmos(CMOS_FLOPPY_INFO);

    k_printf("Master floppy: %s\n", flp_cmos_type[flpinfo >> 4]);
    k_printf("Slave  floppy: %s\n", flp_cmos_type[flpinfo & 0x0F]);

    irq_set_handler(FLOPPY_IRQ, floppy_irq_handler);
    irq_mask(FLOPPY_IRQ, true);

    floppy_dma_init();
    //floppy_cmd();
}
