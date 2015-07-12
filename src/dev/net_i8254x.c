#include <stdlib.h>
#include <stdint.h>
#include <sys/errno.h>

#include <mem/pmem.h>
#include <dev/pci.h>
#include <dev/intrs.h>

#include <cosec/log.h>

#define I8254X_CTRL   0x00    /* NIC control */
#define I8254X_STA    0x08    /* NIC status */
#define I8254X_EERD   0x14    /* EEPROM read */
#define I8254X_ICR    0xc0    /* interrupt cause read */
#define I8254X_IMS    0xd0    /* interrupt mask set/read -- affected by set bits only */
#define I8254X_IMC    0xd8    /* interrupt mask clear  -- affected by set bits only */

#define I8254X_RCTL   0x100   /* receive control */

/* RX descriptors ring buffer */
#define I8254X_RDBAL  0x2800    /* RX descr. array base address (00-31b) */
#define I8254X_RDBAH  0x2804    /* RX descr. array base address (32-63b) */
#define I8254X_RDLEN  0x2808    /* RX descr. array length in bytes */
#define I8254X_RDH    0x2810    /* RX descr. head pointer */
#define I8254X_RDT    0x2818    /* RX descr. tail pointer */

#define I8254X_MTA    0x5200    /* multicast table array, 0x80 entries */

/* CTRL register bits */
#define CTRL_FD       (1u << 0)    /* full-duplex mode */
#define CTRL_LRST     (1u << 3)    /* link reset */
#define CTRL_SLU      (1u << 6)    /* set link up */

/* EERD register bits */
#define EERD_START    (1u << 0)
#define EERD_DONE     (1u << 4)

/* interrupt mask register bits */
#define IM_LSC       (1u << 2)   /* link status change */
#define IM_RXSEQ     (1u << 3)   /* RX sequence error */
#define IM_RXDMT0    (1u << 4)   /* RX descr. minimum threshold hit */
#define IM_RXO       (1u << 6)   /* RX FIFO overrun */
#define IM_RXT0      (1u << 7)   /* RX timer interrupt */
#define IM_MDAC      (1u << 9)   /* MDIO access complete */
#define IM_RXCFG     (1u << 10)  /* receiving ordered sets (?) */
#define IM_PHYINT    (1u << 12)  /* PHY interrupt */
#define IM_GPI0      (1u << 13)
#define IM_GPI1      (1u << 14)  /* general purpose interrupts */
#define IM_TXD_LOW   (1u << 15)  /* TX descr. low threshold hit */
#define IM_SRPD      (1u << 16)  /* small receive pocket detected and transferred */

#define INTRMASK_ALL   ( IM_LSC | IM_RXSEQ | IM_RXDMT0 | IM_MDAC | IM_RXCFG \
                       | IM_PHYINT | IM_GPI0 | IM_GPI1 | IM_TXD_LOW | IM_SRPD)

/* RX control */
#define RCTL_EN     (1u << 1)   /* RX enable */
#define RCTL_BSIZE00   0x00000000   /* 2KB, BSEX must be 0 */
#define RCTL_BSIZE01   0x00010000   /* 1KB  or 16KB for BSEX=0/1 */
#define RCTL_BSIZE10   0x00020000   /* 512B or 8KB  for BSEX=0/1 */
#define RCTL_BSIZE11   0x00030000   /* 256B or 4KB  for BSEX=0/1 */
#define RCTL_VFE    (1u << 18)  /* VLAN filter enable */
#define RCTL_DPF    (1u << 22)  /* discard PAUSE frames */
#define RCTL_BSEX   (1u << 25)  /* RX buffer size extension */
#define RCTL_SECRC  (1u << 26)  /* strip Ethernet CRC from frames */


#define ETH_BUFSZ           2048

#define NUM_DESCR_PAGES     3     /* pages per descriptor table */
#define NUM_RX_DESCRIPTORS  (PAGE_SIZE * NUM_DESCR_PAGES / sizeof(i825xx_rx_desc_t))
#define NUM_TX_DESCRIPTORS  (PAGE_SIZE * NUM_DESCR_PAGES / sizeof(i825xx_tx_desc_t))

// RX and TX descriptor structures
typedef struct __packed i825xx_rx_desc_s {
    volatile uint64_t   address;

    volatile uint16_t   length;
    volatile uint16_t   checksum;
    volatile uint8_t    status;
    volatile uint8_t    errors;
    volatile uint16_t   special;
} i825xx_rx_desc_t;

typedef struct __packed i825xx_tx_desc_s {
    volatile uint64_t   address;

    volatile uint16_t   length;
    volatile uint8_t    cso;
    volatile uint8_t    cmd;
    volatile uint8_t    sta;
    volatile uint8_t    css;
    volatile uint16_t   special;
} i825xx_tx_desc_t;

typedef struct {
    uint32_t  hwid;
    char     *mmio_addr;
    uint8_t   mac_addr[6];
    uint8_t   intr;

    /* RX descriptors ring buffer */
    volatile i825xx_rx_desc_t *rxda;
    volatile uint16_t         rx_tail;

    /* TX descriptors ring buffer */
    volatile i825xx_tx_desc_t *txda;
    volatile uint16_t         tx_tail;
} i8254x_nic;


/*
 *    MMIO
 */

static inline uint32_t *
mmio_port(i8254x_nic *nic, uint32_t offset) {
    return (uint32_t *)(nic->mmio_addr + offset);
}

static inline uint32_t
mmio_read(i8254x_nic *nic, uint32_t offset) {
    return *(uint32_t *)(nic->mmio_addr + offset);
}

static inline void
mmio_write(i8254x_nic *nic, uint32_t offset, uint32_t val) {
    *(uint32_t *)(nic->mmio_addr + offset) = val;
}


/*
 *
 */
uint16_t net_i8254x_read_eeprom(i8254x_nic *nic, uint8_t addr) {
    volatile uint32_t *eerd = (uint32_t *)(nic->mmio_addr + I8254X_EERD);
    *eerd = ((uint32_t)(addr) << 8);
    *eerd = EERD_START | ((uint32_t)(addr) << 8);

    uint32_t tmp = 0;
    do {
        tmp = *eerd;
    } while (!(tmp & EERD_DONE));

    return (uint16_t)((tmp >> 16) & 0xffff);
}

static void i8254x_read_mac_addr(i8254x_nic *nic) {
    uint16_t mac0 = net_i8254x_read_eeprom(nic, 0);
    uint16_t mac1 = net_i8254x_read_eeprom(nic, 1);
    uint16_t mac2 = net_i8254x_read_eeprom(nic, 2);
    nic->mac_addr[0] = mac0 & 0xff;
    nic->mac_addr[1] = (uint8_t)(mac0 >> 8);
    nic->mac_addr[2] = mac1 & 0xff;
    nic->mac_addr[3] = (uint8_t)(mac1 >> 8);
    nic->mac_addr[4] = mac2 & 0xff;
    nic->mac_addr[5] = (uint8_t)(mac2 >> 8);
}

static inline void i8254x_mta_init(i8254x_nic *nic) {
    int i;  /* no memset() here, it must be dword access */
    for (i = 0; i < 0x80; ++i)
        mmio_write(nic, I8254X_MTA + 4 * i, 0);
}

int i8254x_rx_init(i8254x_nic *nic) {
    const char *funcname = __FUNCTION__;
    const uint32_t packet_bufsz = 8192 + 16;
    size_t i;

    void *rxda = pmem_alloc(NUM_DESCR_PAGES); /* must be 16 bytes aligned */
    assert(rxda, ENOMEM, "%s: pmem_alloc(rxdescrs) failed\n", funcname);
    nic->rxda = (volatile i825xx_rx_desc_t *)rxda;

    size_t n_rxbuf_pages = NUM_RX_DESCRIPTORS * ETH_BUFSZ / PAGE_SIZE;
    if (NUM_RX_DESCRIPTORS * ETH_BUFSZ % PAGE_SIZE) ++n_rxbuf_pages;
    uint8_t *rxbufs = pmem_alloc(n_rxbuf_pages);
    assert(rxbufs, ENOMEM, "%s: pmem_alloc(rxbufs) failed\n", funcname);
    logmsgf("[%x]: rxbuf = *%x (%d pages)\n", nic->hwid, (uint)rxbufs, n_rxbuf_pages);

    for (i = 0; i < NUM_RX_DESCRIPTORS; ++i) {
        nic->rxda[i].address = (uint64_t)(rxbufs + i * ETH_BUFSZ);
        nic->rxda[i].status = 0;
    }

    /* rx array base address */
    mmio_write(nic, I8254X_RDBAL, (uint32_t)rxda);
    mmio_write(nic, I8254X_RDBAH, 0);
    logmsgf("[%x]: rxda = *%x[ %d rxdescr ]\n", nic->hwid, (uint)rxda, NUM_RX_DESCRIPTORS);

    /* rx descr. array length in bytes */
    mmio_write(nic, I8254X_RDLEN, NUM_RX_DESCRIPTORS * sizeof(i825xx_rx_desc_t));

    /* setup head and tail pointers */
    mmio_write(nic, I8254X_RDH, 0);
    mmio_write(nic, I8254X_RDT, NUM_RX_DESCRIPTORS);
    nic->rx_tail = 0;

    /* RX control */
    uint32_t bsize = RCTL_BSIZE00;
    if (ETH_BUFSZ < 256) bsize = RCTL_BSIZE11;
    else if (ETH_BUFSZ < 512) bsize = RCTL_BSIZE10;
    else if (ETH_BUFSZ < 1024) bsize = RCTL_BSIZE01;
    else if (ETH_BUFSZ < 2048) bsize = RCTL_BSIZE00;
    else if (ETH_BUFSZ < 4096) bsize = RCTL_BSIZE11 | RCTL_BSEX;
    else if (ETH_BUFSZ < 8192) bsize = RCTL_BSIZE10 | RCTL_BSEX;
    else if (ETH_BUFSZ < 16384) bsize = RCTL_BSIZE01 | RCTL_BSEX;
    else return EINVAL;
    mmio_write(nic, I8254X_RCTL, RCTL_EN | RCTL_SECRC | bsize);

    return 0;
}

int i8254x_tx_init(i8254x_nic *nic) {
    const char *funcname = __FUNCTION__;
    int i;

    void *txda = pmem_alloc(NUM_DESCR_PAGES);
    assert(txda, ENOMEM, "%s: pmem_alloc() failed\n", funcname);

    nic->txda = txda;
    return 0;
}


i8254x_nic theI8254NIC = {
    .mmio_addr = NULL,
    .rxda = NULL,
    .txda = NULL,
    .intr = 0xff,
};

/*
 *    the interrupt handler
 */
void i8254x_irq() {
    logmsgf("#nic\n");
}


int net_i8254x_init(pci_config_t *conf) {
    const char *funcname = __FUNCTION__;
    int ret;

    assert(conf->pci_bar0.val, -ENXIO, "%s: bar0 is %x",
           funcname, conf->pci_bar0.val);
    assert(conf->pci_bar0.not_mmio, -EINVAL, "%s: bar0 is port", funcname);
    assert(theI8254NIC.mmio_addr == NULL,
           -EAGAIN, "%s: only one instance is supported, TODO", funcname);

    theI8254NIC.mmio_addr = (char *)(conf->pci_bar0.val & 0xfffffff0);
    i8254x_read_mac_addr(&theI8254NIC);

    theI8254NIC.intr = conf->pci_interrupt_line;
    theI8254NIC.hwid = ((uint32_t)conf->pci.vendor << 16) | (uint32_t)conf->pci.device;

    k_printf("82540EM: mmio at *%x, intr #%d, mac=%x:%x:%x:%x:%x:%x\n",
             theI8254NIC.mmio_addr, (uint32_t)theI8254NIC.intr,
             (uint)theI8254NIC.mac_addr[0], (uint)theI8254NIC.mac_addr[1],
             (uint)theI8254NIC.mac_addr[2], (uint)theI8254NIC.mac_addr[3],
             (uint)theI8254NIC.mac_addr[4], (uint)theI8254NIC.mac_addr[5]);

    /* set link up */
    mmio_write(&theI8254NIC, I8254X_CTRL,
               mmio_read(&theI8254NIC, I8254X_CTRL) | CTRL_SLU);

    i8254x_mta_init(&theI8254NIC);

    /* enable all interrupts and clear pending ones */
    mmio_write(&theI8254NIC, I8254X_IMS, INTRMASK_ALL);
    mmio_read(&theI8254NIC, I8254X_ICR);

    ret = i8254x_rx_init(&theI8254NIC);
    assert(ret == 0, ret, "%s: i8254x_rx_init() failed(%d)\n", funcname, ret);
    ret = i8254x_tx_init(&theI8254NIC);
    assert(ret == 0, ret, "%s: i8254x_tx_init() failed(%d)\n", funcname, ret);

    irq_set_handler(theI8254NIC.intr, i8254x_irq);
    irq_mask(theI8254NIC.intr, true);

    return 0;
}

