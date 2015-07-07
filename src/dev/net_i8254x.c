#include <stdlib.h>
#include <stdint.h>
#include <sys/errno.h>

#include <dev/pci.h>

#include <cosec/log.h>

#define I8254X_CTRL   0x00
#define I8254X_STA    0x08
#define I8254X_EERD   0x14

#define NUM_RX_DESCRIPTORS	768
#define NUM_TX_DESCRIPTORS	768

typedef struct {
    char *mmio_addr;
    uint8_t mac_addr[6];
} i8254x_nic;

// RX and TX descriptor structures
typedef struct __packed i825xx_rx_desc_s {
	volatile uint64_t	address;
	
	volatile uint16_t	length;
	volatile uint16_t	checksum;
	volatile uint8_t	status;
	volatile uint8_t	errors;
	volatile uint16_t	special;
} i825xx_rx_desc_t;

typedef struct __packed i825xx_tx_desc_s {
	volatile uint64_t	address;
	
	volatile uint16_t	length;
	volatile uint8_t	cso;
	volatile uint8_t	cmd;
	volatile uint8_t	sta;
	volatile uint8_t	css;
	volatile uint16_t	special;
} i825xx_tx_desc_t;


#define EERD_START  1
#define EERD_DONE   4

uint16_t net_i8254x_read_eeprom(i8254x_nic *nic, uint8_t addr) {
    volatile uint32_t *eerd = (uint32_t *)(nic->mmio_addr + I8254X_EERD);
    *eerd = ((uint32_t)(addr) << 8);
    *eerd = EERD_START | ((uint32_t)(addr) << 8);

    uint32_t tmp = 0;
    do {
        tmp = *eerd;
    } while (!(tmp & (1 << EERD_DONE)));

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


i8254x_nic theI8254NIC = {
    .mmio_addr = NULL,
};

int net_i8254x_init(pci_config_t *conf) {
    const char *funcname = __FUNCTION__;
    assert(conf->pci_bar0.val, -ENXIO, "%s: bar0 is %x",
           funcname, conf->pci_bar0.val);
    assert(conf->pci_bar0.not_mmio, -EINVAL, "%s: bar0 is port", funcname);
    assert(theI8254NIC.mmio_addr == NULL,
           -EAGAIN, "%s: only one instance is supported, TODO", funcname);

    theI8254NIC.mmio_addr = (char *)(conf->pci_bar0.val & 0xfffffff0);
    i8254x_read_mac_addr(&theI8254NIC);

    k_printf("82540EM: mmio at *%x, intr #%d, mac=%x:%x:%x:%x:%x:%x\n",
             theI8254NIC.mmio_addr, (uint32_t)conf->pci_interrupt_line,
             (uint)theI8254NIC.mac_addr[0], (uint)theI8254NIC.mac_addr[1],
             (uint)theI8254NIC.mac_addr[2], (uint)theI8254NIC.mac_addr[3],
             (uint)theI8254NIC.mac_addr[4], (uint)theI8254NIC.mac_addr[5]);

    return -ETODO;
}

