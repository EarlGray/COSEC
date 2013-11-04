#include <dev/pci.h>

#include <arch/i386.h>
#include <std/stdio.h>

#include <log.h>

#define PCI_CONFIG_ADDR     0x0CF8
#define PCI_CONFIG_DATA     0x0CFC

#define PCI_CONF_ID_OFF         0x0
#define PCI_CONF_STATCMD_OFF    0x04
#define PCI_CONF_CLASS_OFF      0x08
#define PCI_CONF_REV_OFF        0x0a
#define PCI_CONF_BIST_HDR       0x0c
#define PCI_CONF_INTR_OFF       0x3c

const char * pci_class_descriptions[] = {
    "class 0",                  "mass storage controller",  "network controller",   "display controller",       // 0x00 - 0x03
    "multimedia controller",    "memory controller",        "bridge device",        "simple communication controllr",  // 0x04 - 0x07
    "base system peripherals",  "input device",             "docking station",     "processor",            // 0x08-0xB
    "serial bus controller",    "wireless controller",      "intelligent I/O controller",  "class 0xF",
    "cryptocontroller",
};

/*
uint16_t pci_config_read_word(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset) {
    uint address =
}*/

uint pci_config_read_dword(uint bus, uint slot, uint func, uint offset) {
    uint address = ((bus & 0x3F) << 16) | ((slot & 0xF) << 11) | ((func & 0x3) << 8)
        | (offset & 0xFC) | 0x80000000;
    outl(PCI_CONFIG_ADDR, address);
    uint res;
    inl(PCI_CONFIG_DATA, res);
    return res;
}

void pci_info(uint bus, int slot) {
    uint id = pci_config_read_dword(bus, slot, 0, PCI_CONF_ID_OFF);
    if (0xFFFF == (uint16_t)id)
        return;

    printf("[%d:%d] %x:%x", bus, slot,
                (uint)(id & 0xFFFF), (uint)(id >> 16));
    uint dev_clss = pci_config_read_dword(bus, slot, 0, PCI_CONF_CLASS_OFF);
    uint8_t clss = dev_clss >> 24;
    printf(", class %x:%x (%s)\n",
                (uint)clss, (uint)((dev_clss & 0x00FF0000) >> 16),
                (clss < sizeof(pci_class_descriptions)/sizeof(char*) ? pci_class_descriptions[clss] : "?" ));
    uint hdr = pci_config_read_dword(bus, slot, 0, PCI_CONF_BIST_HDR);
    printf("Header type = %d\n", (uint8_t)(hdr >> 16));

    int off = 0;
    for (; off < 0x40; off += 4) {
        char buf[80];
        sprintf(buf, "0x%0.2x: %0.8x\n", off,
                pci_config_read_dword(bus, slot, 0, off));
        printf("%s", buf);
    }
}

void pci_list(uint bus) {
    int slot;

    uint start_dev_id = 0;

    for (slot = 0; slot < 32; ++slot) {
        uint id = pci_config_read_dword(bus, slot, 0, PCI_CONF_ID_OFF);

        if (0xFFFF == (uint16_t)id)
            continue;

        if (start_dev_id == 0) start_dev_id = id;
        else if (start_dev_id == id) break;

        printf("[pci:%d] %x:%x", slot,
                (uint)(id & 0xFFFF), (uint)(id >> 16));

        uint dev_class = pci_config_read_dword(bus, slot, 0, PCI_CONF_CLASS_OFF);
        uint8_t clss = dev_class >> 24;
        printf(", class %x:%x (%s)\n",
                (uint)clss, (uint)((dev_class & 0x00FF0000) >> 16),
                (clss < sizeof(pci_class_descriptions)/sizeof(char*) ? pci_class_descriptions[clss] : "?" ));
    }
}
