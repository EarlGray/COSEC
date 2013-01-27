#include <arch/i386.h>
#include <dev/intrs.h>
#include <dev/cmos.h>

#include <log.h>


#define FLOPPY_DMA_AREA     ((void *)0x90000)

/* 1.44M 3.5" floppy geometry */
#define FLP144_BYTES_PER_SECT   512
#define FLP144_SECT_PER_TRACK   18
#define FLP144_BYTES_PER_TRACK  (FLP144_BYTES_PER_SECT * FLP144_SECT_PER_TRACK)

#define FLOPPY_GAP3_LEN_STD     42
#define FLOPPY_GAP3_5_14        32
#define FLOPPY_GAP3_3_5         27

typedef struct {
    uint8_t cyl;    // 0..79
    uint8_t head;   // 0,1
    uint8_t sector; // 1..18
} chs_t;

/* FDC base port */
#define FDC0    0x3f0
#define FDC1    0x370

/* FDC port offsets: */
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

/* DMA */
#define DMA_CHANNEL1    1
#define DMA_FLOPPY      2
#define DMA_HDD         3

#define DMA_ADDRESS_REG_1       0x02
#define DMA_COUNT_REG_1         0x03
#define DMA_PAGE_ADDR_REG_1     0x83
#define DMA_ADDRESS_REG_2       0x04
#define DMA_COUNT_REG_2         0x05
#define DMA_PAGE_ADDR_REG_2     0x81
#define DMA_ADDRESS_REG_3       0x06
#define DMA_COUNT_REG_3         0x07
#define DMA_PAGE_ADDR_REG_3     0x82
#define DMA_SLAV_STATUS_REG         0x08
#define DMA_SLAV_SINGLE_MASK_PORT   0x0A
#define DMA_SLAV_MODE_REG           0x0B
#define DMA_SLAV_FLIPFLOP_RST_REG   0x0C

#define DMA_ADDRESS_REG_5       0xC4
#define DMA_COUNT_REG_5         0xC6
#define DMA_PAGE_ADDR_REG_5     0x8B
#define DMA_ADDRESS_REG_6       0xC8
#define DMA_COUNT_REG_6         0xCA
#define DMA_PAGE_ADDR_REG_6     0x89
#define DMA_ADDRESS_REG_7       0xCC
#define DMA_COUNT_REG_7         0xCE
#define DMA_PAGE_ADDR_REG_7     0x8A
#define DMA_MAST_ADDRESS_REG_6      0xC8
#define DMA_MAST_COUNT_REG_6        0xCA
#define DMA_MAST_STATUS_REG         0xD0
#define DMA_MAST_SINGLE_MASK_PORT   0xD4
#define DMA_MAST_MODE_REG           0xD6
#define DMA_MAST_FLIPFLOP_RST_REG   0xD8

inline static void dma_mask_channel(uint ch, bool status) {
    switch (ch) {
    case DMA_CHANNEL1: case DMA_FLOPPY: case DMA_HDD:
        outb_p(DMA_SLAV_SINGLE_MASK_PORT,
             ch | (status ? (1<<3) : 0));
        break;
    default: k_printf("Warning: dma_mask_channel(%d)\n", ch);
    }
}

static inline void dma_reset_flipflop(uint ch) {
    if (ch < 4) outb_p(DMA_SLAV_FLIPFLOP_RST_REG, 0xFF);
    else k_printf("Warning: dma_reset_flipflop(%d)\n", ch);
}

static inline void dma_set_addr(uint ch, ptr_t ptr) {
    assertv(ptr < 0x1000000,
           "dma_set_addr: Address must be less than 0x1000000\n");
    uint port, pgport;
    switch (ch) {
    case DMA_CHANNEL1:  port = DMA_ADDRESS_REG_1; pgport = DMA_PAGE_ADDR_REG_1; break;
    case DMA_FLOPPY:    port = DMA_ADDRESS_REG_2; pgport = DMA_PAGE_ADDR_REG_2; break;
    case DMA_HDD:       port = DMA_ADDRESS_REG_3; pgport = DMA_PAGE_ADDR_REG_3; break;
    case 5: port = DMA_ADDRESS_REG_5; pgport = DMA_PAGE_ADDR_REG_5; break;
    case 6: port = DMA_ADDRESS_REG_6; pgport = DMA_PAGE_ADDR_REG_6; break;
    case 7: port = DMA_ADDRESS_REG_7; pgport = DMA_PAGE_ADDR_REG_7; break;
    default:
        k_printf("Warning: dma_set_addr(%d, %x)\n", ch, ptr);
        return;
    }
    dma_reset_flipflop(ch);
    outb_p(port, (ptr & 0xFF));
    outb_p(port, (ptr >> 8) & 0xFF);
    outb_p(pgport, (ptr >> 16) & 0xFF);
}

static inline void dma_set_count(uint ch, size_t sz) {
    assertv(sz < 0x10000,
            "dma_set_count: Size must be less than 0x10000\n");
    uint port;
    switch (ch) {
    case DMA_CHANNEL1:  port = DMA_COUNT_REG_1; break;
    case DMA_FLOPPY:    port = DMA_COUNT_REG_2; break;
    case DMA_HDD:       port = DMA_COUNT_REG_3; break;
    case 5: port = DMA_COUNT_REG_5; break;
    case 6: port = DMA_COUNT_REG_6; break;
    case 7: port = DMA_COUNT_REG_7; break;
    default:
        k_printf("Warning: dma_set_count(%d, %x)\n", ch, sz);
        return;
    }
    dma_reset_flipflop(ch);
    outb_p(port, (sz & 0xFF));
    outb_p(port, (sz >> 8));
}

#define FDC_CMD_MFM     0x40

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
    FDC_CMD_SEEK        =   0xf,
    FDC_CMD_VERSION     =   0x10,
    FDC_CMD_CONFIGURE   =   0x13,
    FDC_CMD_LOCK        =   0x14,
    FDC_CMD_VERIFY      =   0x16
} floppy_cmd_e;

void floppy_dma_init(void) {
    dma_mask_channel(DMA_FLOPPY, true);
    dma_set_addr(DMA_FLOPPY, FLOPPY_DMA_AREA);
    dma_set_count(DMA_FLOPPY, FLP144_BYTES_PER_TRACK - 1);
    dma_mask_channel(DMA_FLOPPY, false);
}

void floppy_dma_prepare_read(void) {
    dma_mask_channel(DMA_FLOPPY, true);
    // single transfer, address increment, autoinit, read, channel 2:
    outb_p(DMA_SLAV_MODE_REG, 0x56);
    dma_mask_channel(DMA_FLOPPY, false);
}

void floppy_dma_prepare_write(void) {
    dma_mask_channel(DMA_FLOPPY, true);
    // single transfer, address increment, autoinit, write, channel 2:
    outb_p(DMA_SLAV_MODE_REG, 0x5a);
    dma_mask_channel(DMA_FLOPPY, false);
}

uint8_t floppy_status(int fdc) {
    uint8_t res = 0;
    inb_p(fdc + FDC_MSR, res);
    return res;
}

void floppy_send_cmd(int fdc, floppy_cmd_e cmd) {
    int i;
    for (i = 0; i < 500; ++i)
        if (floppy_status(fdc) & FDC_MSR_DATAREG_READY) {
            outb_p(FDC_DATA, cmd);
            return;
        }
    k_printf("floppy_send_cmd(%d): failed to send cmd\n", fdc);
}

uint8_t floppy_read_data(int fdc) {
    int i;
    for (i = 0; i < 500; ++i)
        if (floppy_status(fdc) & FDC_MSR_DATAREG_READY) {
            uint8_t res;
            inb_p(fdc + FDC_DATA, res);
            return res;
        }
    k_printf("floppy_read_data(%d): failed to read data\n", fdc);
    return 0;
}

void floppy_irq_handler(void *stack) {
    k_printf("IRQ6\n");
}

void floppy_reset(int fdc) {
    outb(fdc + FDC_DOR, 0);
    usleep(50000);
    outb(fdc + FDC_DOR, FDC_ENABLE | FDC_DMA_MODE);
}

void floppy_cmd_conf(int fdc) {
    floppy_send_cmd(fdc, FDC_CMD_CONFIGURE);
    floppy_send_cmd(fdc, 0);
    // Implied seek ON, FIFO ON, Polling mode OFF, threshold 8
    floppy_send_cmd(fdc, (1<<6) | (1<<4) | 8);
    floppy_send_cmd(fdc, 0);

    /* no result bytes, no interrupt */
}

uint8_t floppy_cmd_version(int fdc) {
    floppy_send_cmd(fdc, FDC_CMD_VERSION);
    usleep(20000);
    return floppy_read_data(fdc);
}

void floppy_turn_on(int fdc, int motor) {
    outb_p(fdc + FDC_DOR, FDC_ENABLE | FDC_DMA_MODE | motor);
}

void lba_to_chs(uint lda, chs_t *chs) {

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

    if ((flpinfo >> 4) == 0) return;

    irq_set_handler(FLOPPY_IRQ, floppy_irq_handler);
    irq_mask(FLOPPY_IRQ, true);

    floppy_reset(FDC0);

    k_printf("floppy reset done\n");
    floppy_dma_init();

    floppy_cmd_conf(FDC0);
    k_printf("floppy_cmd_conf done\n");

    floppy_turn_on(FDC0, FDC_DOR_MOTOR0 | FDC_DOR_DR0);

    uint8_t version = floppy_cmd_version(FDC0);
    k_printf("FDC0 version: %x\n", (uint)version);
}
