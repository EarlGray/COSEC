#ifndef __PCI_H__
#define __PCI_H__

#include <stdint.h>

#include <attrs.h>

typedef union {
    struct {
        uint32_t mmio_addr:28;
        int mmio_prefetchable:1;
        int mmio_64bit:2;
        int not_mmio:1;  // must be 0
    };
    struct {
        uint32_t port_addr:30;
        int rsvrd:1;
        int portio:1;  // must be 1
    };
    uint32_t val;
} pci_bar_t;

typedef struct pci_config {
    union {
        struct {
            uint16_t  vendor;
            uint16_t  device;
        } pci;
        uint32_t pci_id;
    };
    uint16_t  pci_command;
    uint16_t  pci_status;
    uint8_t   pci_rev_id;
    uint8_t   pci_prog_if;
    uint8_t   pci_subclass;
    uint8_t   pci_class;
    uint8_t   pci_cacheline_size;
    uint8_t   pci_latency_timer;
    uint8_t   pci_header_type;
    uint8_t   pci_bist;

    pci_bar_t pci_bar0;
    pci_bar_t pci_bar1;
    pci_bar_t pci_bar2;
    pci_bar_t pci_bar3;
    pci_bar_t pci_bar4;
    pci_bar_t pci_bar5;

    uint32_t  pci_cardbus_ptr;
    uint16_t  pci_subsystem_vendor;
    uint16_t  pci_subsystem_id;

    uint32_t  pci_exprom_baseaddr;
    uint8_t   pci_capabilities;
    uint8_t   pci_rsrvd0;
    uint16_t  pci_rsrvd1;
    uint32_t  pci_rsrvd2;

    uint8_t   pci_interrupt_line;
    uint8_t   pci_interrupt_pin;
    uint8_t   pci_min_grant;
    uint8_t   pci_max_latency;
} pci_config_t;

void pci_list(uint32_t bus);
void pci_info(uint32_t bus, int slot);

void pci_setup(void);

#endif // __PCI_H__
