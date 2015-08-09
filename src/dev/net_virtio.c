#include <dev/pci.h>
#include <arch/i386.h>

#include <cosec/log.h>

int net_virtio_init(pci_config_t *pciconf) {
    const char *funcname = __FUNCTION__;
    uint32_t features = 0;
    uint16_t portbase = 0;

    if (pciconf->pci_bar0.val & 1)
        portbase = pciconf->pci_bar0.val & 0xfffc;
    else if (pciconf->pci_bar1.val & 1)
        portbase = pciconf->pci_bar1.val & 0xfffc;
    else if (pciconf->pci_bar2.val & 1)
        portbase = pciconf->pci_bar2.val & 0xfffc;
    else if (pciconf->pci_bar3.val & 1)
        portbase = pciconf->pci_bar3.val & 0xfffc;
    else if (pciconf->pci_bar4.val & 1)
        portbase = pciconf->pci_bar4.val & 0xfffc;

    return_err_if(portbase == 0, -1, "%s: portbase not found\n", __FUNCTION__);
    k_printf("%s: portbase = 0x%x\n", funcname, (uint)portbase);

    k_printf("%s: features = [", funcname);
    inl(portbase, features);
    if (features & (1u << 0)) k_printf("csumd ");  // partial checksum by device
    if (features & (1u << 1)) k_printf("csumg "); // partial checksum by guest
    if (features & (1u << 5)) {
        uint16_t mac0, mac1, mac2;
        inw(portbase + 0x14, mac0);
        inw(portbase + 0x16, mac1);
        inw(portbase + 0x18, mac2);
        k_printf("mac=%x:%x:%x:%x:%x:%x ",
                 mac0 & 0xff, mac0 >> 8,
                 mac1 & 0xff, mac1 >> 8,
                 mac2 & 0xff, mac2 >> 8);
    }
    if (features & (1u << 7)) k_printf("tsov4g ");
    if (features & (1u << 8)) k_printf("tsov6g ");
    if (features & (1u << 9)) k_printf("tso/ecng ");
    if (features & (1u << 10)) k_printf("ufog ");
    if (features & (1u << 11)) k_printf("tsov4d ");
    if (features & (1u << 15)) k_printf("rbufmrg ");
    if (features & (1u << 16)) {
        uint16_t sta;
        inw(portbase + 0x1a, sta);
        k_printf("sta=%x ", sta);
    }
    if (features & (1u << 17)) k_printf("ctlvq ");
    if (features & (1u << 18)) k_printf("ctlrx ");
    if (features & (1u << 19)) k_printf("vlan ");
    k_printf("]\n");

    return 0;
}
