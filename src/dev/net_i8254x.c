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
#define I8254X_TCTL   0x400   /* transmit control */

#define I8254X_TIPG   0x410   /* transmit inter packet gap */

/* RX descriptors ring buffer registers */
#define I8254X_RDBAL  0x2800    /* RX descr. array base address (00-31b) */
#define I8254X_RDBAH  0x2804    /* RX descr. array base address (32-63b) */
#define I8254X_RDLEN  0x2808    /* RX descr. array length in bytes */
#define I8254X_RDH    0x2810    /* RX descr. head pointer */
#define I8254X_RDT    0x2818    /* RX descr. tail pointer */
#define I8254X_RDTR   0x2820    /* RX delay timer */
#define I8254X_RADV   0x282c    /* RX absolute delay timer */
#define I8254X_RSRPD  0x2c00    /* small packet received */

#define I8254X_MTA    0x5200    /* multicast table array, 0x80 entries */

/* TX descriptors ring buffer registers */
#define I8254X_TDBAL  0x3800    /* TX descr. array base address (00-31b) */
#define I8254X_TDBAH  0x3804    /* TX descr. array base address (32-63b) */
#define I8254X_TDLEN  0x3808    /* TX descr. array length in bytes */
#define I8254X_TDH    0x3810    /* TX descr. head pointer */
#define I8254X_TDT    0x3818    /* TX descr. tail pointer */

/* CTRL register bits */
#define CTRL_FD       (1u << 0)    /* full-duplex mode */
#define CTRL_LRST     (1u << 3)    /* link reset */
#define CTRL_SLU      (1u << 6)    /* set link up */

/* EERD register bits */
#define EERD_START    (1u << 0)
#define EERD_DONE     (1u << 4)

/* interrupt mask register bits */
#define IM_TXDW      (1u << 0)   /* TX descr. written back */
#define IM_TXQE      (1u << 1)   /* TX queue is empty */
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

#define INTRMASK_ALL ( IM_LSC | IM_RXSEQ | IM_RXDMT0 | IM_MDAC | IM_RXCFG \
                       | IM_PHYINT | IM_GPI0 | IM_GPI1 | IM_TXD_LOW | IM_SRPD)
#define INTRMASK_OK  (IM_RXT0 | IM_RXDMT0 | IM_LSC | IM_RXO | IM_RXSEQ)

/* Interrupt Cause bits */

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

/* TX control */
#define TCTL_EN     (1u << 1)   /* TX enable */
#define TCTL_PSP    (1u << 3)   /* TX: pad short packets to 64B */
#define TCTL_CT       (0x0fu << 4)  /* TX: preferred number of retransmission attempts */
#define TCTL_COLD_FD  (0x40u << 12) /* TX: preferred 64-byte time for CSMA/CD */
#define TCTL_RTLC   (1u << 24)  /* TX: retransmit on Late Collision */

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

static inline volatile uint32_t *
mmio_port(i8254x_nic *nic, uint32_t offset) {
    return (volatile uint32_t *)(nic->mmio_addr + offset);
}

inline uint32_t
mmio_read(i8254x_nic *nic, uint32_t offset) {
    uint32_t val = *(volatile uint32_t *)(nic->mmio_addr + offset);
    return val;
}

inline void
mmio_write(i8254x_nic *nic, uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(nic->mmio_addr + offset) = val;
}


/*
 *
 */
uint16_t net_i8254x_read_eeprom(i8254x_nic *nic, uint8_t addr) {
    volatile uint32_t *eerd = mmio_port(nic, I8254X_EERD);
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
        nic->rxda[i].address = (uint64_t)(uint32_t)(rxbufs + i * ETH_BUFSZ);
        nic->rxda[i].status = 0;
    }

    /* rx array base address */
    mmio_write(nic, I8254X_RDBAL, (uint32_t)rxda);
    mmio_write(nic, I8254X_RDBAH, 0);
    logmsgf("[%x]: rxda = *%x[ %d rxdescr ]\n", nic->hwid, (uint)rxda, NUM_RX_DESCRIPTORS);

    /* rx descr. array length in bytes */
    assert(NUM_RX_DESCRIPTORS * sizeof(i825xx_rx_desc_t) % 128 == 0,
           EINVAL, "must be 128B-aligned");
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
    mmio_write(nic, I8254X_RDTR, 0);
    mmio_write(nic, I8254X_RADV, 0);
    mmio_write(nic, I8254X_RSRPD, 0);

    return 0;
}

int i8254x_tx_init(i8254x_nic *nic) {
    const char *funcname = __FUNCTION__;
    size_t i;

    void *txda = pmem_alloc(NUM_DESCR_PAGES);
    assert(txda, ENOMEM, "%s: pmem_alloc(txda) failed\n", funcname);
    nic->txda = (volatile i825xx_tx_desc_t *)txda;

    for (i = 0; i < NUM_TX_DESCRIPTORS; ++i) {
        nic->txda[i].address = 0;
        nic->txda[i].sta = 0;
        nic->txda[i].cmd = 0;
    }

    /* setup TX descr. ring buffer */
    mmio_write(nic, I8254X_TDBAL, (uint32_t)txda);
    mmio_write(nic, I8254X_TDBAH, 0);
    logmsgf("[%x]: txda = *%x[ %d txdescr ]\n", nic->hwid, (uint)txda, NUM_TX_DESCRIPTORS);

    /* TX descr. ring buffer length in bytes */
    mmio_write(nic, I8254X_TDLEN, NUM_TX_DESCRIPTORS * sizeof(i825xx_tx_desc_t));

    mmio_write(nic, I8254X_TDH, 0);
    mmio_write(nic, I8254X_TDT, 0); //NUM_TX_DESCRIPTORS - 1);
    nic->tx_tail = 0;

    mmio_write(nic, I8254X_TCTL, TCTL_EN | TCTL_PSP | TCTL_COLD_FD | TCTL_RTLC);
    mmio_write(nic, I8254X_TIPG, 0x0060200a);
    return 0;
}

void i8254x_rx_poll(i8254x_nic *nic) {
    logmsgif("[%x]: packets pending, TODO\n", nic->hwid);
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
    i8254x_nic *nic = &theI8254NIC; /* TODO */

    uint32_t icr = mmio_read(nic, I8254X_ICR);
    logmsgif("#[%x]: icr=%x", nic->hwid, icr);

    if (icr & IM_LSC) {
        /* link set up! */
        icr &= ~IM_LSC;
        mmio_write(nic, I8254X_CTRL, mmio_read(nic, I8254X_CTRL) | CTRL_SLU);

        logmsgif("[%x]: link status change, STA=0x%x",
                 nic->hwid, mmio_read(nic, I8254X_STA));
    }
    if ((icr & IM_RXDMT0) || (icr & IM_RXO)) {
        /* RX underrun */
        icr &= ~(IM_RXDMT0 | IM_RXO);
        logmsgif("[%x]: RX underrun, rx_head = %d, rx_tail = %x\n",
                 nic->hwid, mmio_read(nic, I8254X_RDH), nic->rx_tail);
    }
    if (icr & IM_RXT0) {
        /* a packet is pending */
        icr &= ~IM_RXT0;
        i8254x_rx_poll(nic);
    }

    if (icr)
        logmsgif("[%x]: unhandled interrupts, ICR=%x\n", icr);

    mmio_read(nic, I8254X_ICR);
}


int net_i8254x_init(pci_config_t *conf) {
    const char *funcname = __FUNCTION__;
    int ret;

    assert(conf->pci_bar0.val, -ENXIO, "%s: bar0 is %x",
           funcname, conf->pci_bar0.val);
    assert(conf->pci_bar0.not_mmio, -EINVAL, "%s: bar0 is port", funcname);
    assert(theI8254NIC.mmio_addr == NULL,
           -EAGAIN, "%s: only one instance is supported, TODO", funcname);

    theI8254NIC.hwid = ((uint32_t)conf->pci.vendor << 16) | (uint32_t)conf->pci.device;
    theI8254NIC.intr = conf->pci_interrupt_line;
    theI8254NIC.mmio_addr = (char *)(conf->pci_bar0.val & 0xfffffff0);

    i8254x_read_mac_addr(&theI8254NIC);
    logmsgif("[%x]: mmio at *%x, intr #%d, mac=%x:%x:%x:%x:%x:%x",
             theI8254NIC.hwid, theI8254NIC.mmio_addr, (uint32_t)theI8254NIC.intr,
             (uint)theI8254NIC.mac_addr[0], (uint)theI8254NIC.mac_addr[1],
             (uint)theI8254NIC.mac_addr[2], (uint)theI8254NIC.mac_addr[3],
             (uint)theI8254NIC.mac_addr[4], (uint)theI8254NIC.mac_addr[5]);

    /* set link up */
    mmio_write(&theI8254NIC, I8254X_CTRL,
               mmio_read(&theI8254NIC, I8254X_CTRL) | CTRL_SLU);

    i8254x_mta_init(&theI8254NIC);

    irq_set_handler(theI8254NIC.intr, i8254x_irq);
    irq_mask(theI8254NIC.intr, true);

    /* enable all interrupts and clear pending ones */
    mmio_write(&theI8254NIC, I8254X_IMC, INTRMASK_ALL);
    mmio_read(&theI8254NIC, I8254X_ICR);

    mmio_write(&theI8254NIC, I8254X_IMS, INTRMASK_OK);
    mmio_read(&theI8254NIC, I8254X_ICR);

    ret = i8254x_rx_init(&theI8254NIC);
    assert(ret == 0, ret, "%s: i8254x_rx_init() failed(%d)\n", funcname, ret);
    ret = i8254x_tx_init(&theI8254NIC);
    assert(ret == 0, ret, "%s: i8254x_tx_init() failed(%d)\n", funcname, ret);

    /*
    k_printf("[%x] CTL = %x\n", theI8254NIC.hwid, mmio_read(&theI8254NIC, I8254X_CTRL));
    k_printf("[%x] STA = %x\n", theI8254NIC.hwid, mmio_read(&theI8254NIC, I8254X_STA));
    k_printf("[%x] IMS = %x\n", theI8254NIC.hwid, mmio_read(&theI8254NIC, I8254X_IMS));
    */

    return 0;
}

