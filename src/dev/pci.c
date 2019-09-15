#include <dev/pci.h>
#include <arch/i386.h>

#include <stdlib.h>
#include <stdio.h>
#include <cosec/log.h>

#define PCI_CONFIG_ADDR     0x0CF8
#define PCI_CONFIG_DATA     0x0CFC

#define PCI_CONF_ID_OFF         0x0
#define PCI_CONF_STATCMD_OFF    0x04
#define PCI_CONF_CLASS_OFF      0x08
#define PCI_CONF_REV_OFF        0x0a
#define PCI_CONF_BIST_HDR       0x0c
#define PCI_CONF_INTR_OFF       0x3c

const char * pci_class_descriptions[] = {
    "class 0",
    "mass storage controller",
    "network controller",
    "display controller",       // 0x00 - 0x03
    "multimedia controller",
    "memory controller",
    "bridge device",
    "simple communication controller", // 0x04 - 0x07
    "base system peripherals",
    "input device",
    "docking station",
    "processor",            // 0x08-0xB
    "serial bus controller",
    "wireless controller",
    "intelligent I/O controller",
    "class 0xF",
    "cryptocontroller",
};

typedef struct {
    uint32_t pci_id;
    int (*pci_init)(pci_config_t *);
    const char *pci_name;
} pci_driver_t;

extern int net_i8254x_init(pci_config_t *);
extern int net_virtio_init(pci_config_t *);

const pci_driver_t pci_driver[] = {
    /*
    { .pci_id = 0x100e8086,
      .pci_init = net_i8254x_init,
      .pci_name = "Intel 82540EM Gigabit Ethernet" },
    */

    { .pci_id = 0x10001af4,
      .pci_init = net_virtio_init,
      .pci_name = "VirtIO network card" },

    { .pci_init = NULL }
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

void pci_read_config(uint bus, uint slot, pci_config_t *conf) {
    assertv(sizeof(pci_config_t)/sizeof(uint32_t) == 0x10,
            "pci_config_t size is invalid");
    uint32_t *val = (uint32_t *)conf;

    int i;
    for (i = 0; i < 0x10; ++i) {
        uint32_t v = pci_config_read_dword(bus, slot, 0, i * sizeof(uint32_t));
        val[i] = v;
    }
}

void pci_info(uint bus, int slot) {
    uint id = pci_config_read_dword(bus, slot, 0, PCI_CONF_ID_OFF);
    if (0xFFFF == (uint16_t)id)
        return;

    printf("[%d:%d] %04x:%04x", bus, slot,
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
        sprintf(buf, "0x%02x: %08x\n", off,
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

        printf("[pci:%d] %04x:%04x", slot,
                (uint)(id & 0xFFFF), (uint)(id >> 16));

        uint dev_class = pci_config_read_dword(bus, slot, 0, PCI_CONF_CLASS_OFF);
        uint8_t clss = dev_class >> 24;
        printf(", class %x:%x (%s)\n",
                (uint32_t)clss, (uint32_t)((dev_class >> 16) & 0x00FF),
                (clss < sizeof(pci_class_descriptions)/sizeof(char*) ? pci_class_descriptions[clss] : "?" ));
    }
}

static const pci_driver_t * lookup_pci_driver(uint32_t id) {
    size_t i;
    const pci_driver_t *cur = pci_driver;
    while (cur->pci_init) {
        if (cur->pci_id == id)
            return cur;
        ++cur;
    }
    return NULL;
}

static pci_bus_setup(int bus) {
    int slot;
    pci_config_t conf;

    uint32_t loop_id = 0;
    for (slot = 0; slot < 32; ++slot) {
        pci_read_config(bus, slot, &conf);
        if (conf.pci.device == 0xffff)
            continue;
        if (loop_id == 0)
            loop_id = conf.pci_id;
        else if (loop_id == conf.pci_id)
            break;

        const char *desc = "unknown device type";

        const pci_driver_t *drv = lookup_pci_driver(conf.pci_id);
        if (!drv) {
            if (conf.pci_class < sizeof(pci_class_descriptions)/sizeof(char*))
                desc = pci_class_descriptions[ conf.pci_class ];

            k_printf("pci:%d:%d\t%x:%x (intr %d:%d) - %s\n", bus, slot,
                   conf.pci.vendor, conf.pci.device,
                   conf.pci_interrupt_pin, conf.pci_interrupt_line,
                   desc);
            continue;
        }

        k_printf("pci:%d:%d\t%x:%x (intr %d:%d) - %s\n", bus, slot,
               conf.pci.vendor, conf.pci.device,
               conf.pci_interrupt_pin, conf.pci_interrupt_line,
               drv->pci_name);

        int ret = drv->pci_init(&conf);
        if (ret) {
            k_printf("[%x:%x] init error: %s\n",
                     conf.pci.vendor, conf.pci.device, strerror(-ret));
        }
    }
}

void pci_setup(void) {
    pci_bus_setup(0);
    pci_bus_setup(1);
    pci_bus_setup(2);
}
