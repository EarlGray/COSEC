#include <dev/pci.h>

#include <arch/i386.h>

#define PCI_CONFIG_ADDR     0x0CF8
#define PCI_CONFIG_DATA     0x0CFC

const char * pci_class_descriptions[] = {
    "0",   "mass storage controller",   "network controller",   "display controller",       // 0x00 - 0x03
    "multimedia controller",    "memory controller",    "bridge device",    "simple communication controllr",  // 0x04 - 0x07
    "base system peripherals",  "input device",   "docking station",     "processor",            // 0x08-0xB
    "serial bus controller", "wireless controller", "intelligent I/O controller",  "",
    "enryption/decryption controller", 
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

void pci_setup(void) {
    int slot;
    int bus = 0;

    uint start_dev_id = 0;

    for (slot = 0; ; ++slot) {
        uint id = pci_config_read_dword(bus, slot, 0, 0);

        if (0xFFFF == (uint16_t)id) 
            continue;

        if (start_dev_id == 0) start_dev_id = id;
        else if (start_dev_id == id) break;

        k_printf("[pci:%d] %x:%x", slot,
                (uint)(id & 0xFFFF), (uint)(id >> 16));

        uint dev_class = pci_config_read_dword(bus, slot, 0, 8);
        uint8_t clss = dev_class >> 24;
        k_printf(", class %x:%x (%s)\n", 
                (uint)clss, (uint)((dev_class & 0x00FF0000) >> 16),
                (clss < sizeof(pci_class_descriptions)/sizeof(char*) ? pci_class_descriptions[clss] : "?" ));
    }  
}
