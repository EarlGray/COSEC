#include <sys/errno.h>

#include <dev/pci.h>
#include <arch/i386.h>

#include <attrs.h>
#include <cosec/log.h>

/* virtio registers offsets from `portbase` */
#define VIO_DEV_FEATURE       0   /* 32, R  */
#define VIO_DRV_FEATURE       4   /* 32, RW */
#define VIO_Q_ADDR            8   /* 32, RW */
#define VIO_Q_SIZE           12   /* 16, R  */
#define VIO_Q_SELECT         14   /* 16, RW */
#define VIO_Q_NOTIFY         16   /* 16, RW */
#define VIO_DEV_STA          18   /*  8, RW */
#define VIO_ISR_STA          19   /*  8, R  */
#define VIO_DEVICE_SPECIFIC_OFFSET    20

#define VIO_MSIX_CONF_VECT   20   /* 16, RW */
#define VIO_MSIX_Q_VECT      22   /* 16, RW */
#define VIO_DEVICE_SPECIFIC_OFFSET_WITH_MSIX  24

#define VIO_NET_MAC           0   /* 48, R, if VIRTIO_NET_F_MAC */
#define VIO_NET_STA           6   /* 16, R, if VIRTIO_NET_F_STATUS */

/* may be written into VIO_MSIX_CONF_VECT */
#define VIRTIO_MSI_NO_VECTOR  0xffff

/* VIO_D??_FEATURE register */
enum vio_features {
    /* notify on empty avail_ring even if supppressed */
    VIRTIO_F_NOTIFY_ON_EMPTY  = (1u << 24),
    /* enable VIRTQ_DESC_F_INDIR */
    VIRTIO_F_RING_INDIR_DESC  = (1u << 28),
    /* enable `used_event` and `avail_event` */
    VIRTIO_F_RING_EVENT_IDX   = (1u << 29),
};

/* VIO_D??_FEATURE for a network device */
enum vio_net_features {
    /* device handles packets with a partual checksum */
    VIRTIO_NET_F_CSUM       = (1u << 0),
    /* guest handles packets with a partual checksum */
    VIRTIO_NET_F_GUEST_CSUM = (1u << 1),
    /* device has given MAC address */
    VIRTIO_NET_F_MAC        = (1u << 5),
    VIRTIO_NET_F_GUEST_TSOV4 = (1u << 7),
    VIRTIO_NET_F_GUEST_TSOV6 = (1u << 8),
    VIRTIO_NET_F_GUEST_ECN  = (1u << 9),
    VIRTIO_NET_F_GUEST_UFO  = (1u << 10),
    VIRTIO_NET_F_HOST_TSO4  = (1u << 11),
    VIRTIO_NET_F_HOST_TSO6  = (1u << 12),
    VIRTIO_NET_F_HOST_ECN   = (1u << 13),
    VIRTIO_NET_F_HOST_UFO   = (1u << 14),
    VIRTIO_NET_F_MRG_RXBUF  = (1u << 15),
    VIRTIO_NET_F_STATUS     = (1u << 16),
    VIRTIO_NET_F_CTRL_VQ    = (1u << 17),
    VIRTIO_NET_F_CTRL_RX    = (1u << 18),
    VIRTIO_NET_F_CTRL_VLAN  = (1u << 19),
    VIRTIO_NET_F_GUEST_ANNOUNCE = (1u << 21),
};

/* VIO_DEV_STA register */
enum vio_dev_sta {
    STA_ACK           = 0x01,
    STA_DRV           = 0x02,
    STA_DRV_OK        = 0x04,
    STA_FAILED        = 0x80,
};

enum virtq_desc_flags {
    VIRTQ_DESC_F_NEXT  = 1,
    VIRTQ_DESC_F_WRITE = 2,
    VIRTQ_DESC_F_INDIR = 4,
};

struct __packed virtq_desc {
    uint64_t addr;
    uint32_t len;

    uint16_t flags; /* see: enum virtq_desc_flags */
    uint16_t next;  /* if:  flags & VIRTQ_DESC_F_NEXT */
};

#define VIRTQ_AVAIL_F_NOINTR  1
struct __packed virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
};


enum vio_net_hdr_flags {
    VIRTIO_NET_HDR_F_NEEDS_CSUM = 0x01,
};
enum vio_net_gso {
    VIRTIO_NET_HDR_GSO_NONE     = 0,
    VIRTIO_NET_HDR_GSO_TCPv4    = 1,
    VIRTIO_NET_HDR_GSO_UDP      = 3,
    VIRTIO_NET_HDR_GSO_TCPv6    = 4,
    VIRTIO_NET_HDR_GSO_ECN      = 5,
};

struct __packed virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers; /* if VIRTIO_NET_F_MRG_RXBUF */
};

static int net_virtio_setup() {
    return 0;
}

int net_virtio_init(pci_config_t *pciconf) {
    const char *funcname = __FUNCTION__;
    uint32_t features = 0;
    uint16_t portbase = 0;

    return_msg_if(pciconf->pci_rev_id > 0, -ENOSYS,
                  "%s: pci->rev_id=%d, aborting configuration\n",
                  __FUNCTION__, pciconf->pci_rev_id);
    assert(pciconf->pci_sybsyst_id == 1, -EINVAL,
           "%s: pci->subsystem_id is not VIRTIO_NET\n", __FUNCTION__);

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
    k_printf("%s: portbase = 0x%x, intr = #%d\n", funcname,
             (uint)portbase, pciconf->pci_interrupt_line);

    k_printf("%s: [", funcname);
    inl(portbase, features);
    if (features & VIRTIO_NET_F_CSUM) k_printf("csumd ");
    if (features & VIRTIO_NET_F_GUEST_CSUM) k_printf("csumg ");
    if (features & VIRTIO_NET_F_MAC) {
        uint16_t mac0, mac1, mac2;
        inw(portbase + 0x14, mac0);
        inw(portbase + 0x16, mac1);
        inw(portbase + 0x18, mac2);
        k_printf("mac=%x:%x:%x:%x:%x:%x ",
                 mac0 & 0xff, mac0 >> 8,
                 mac1 & 0xff, mac1 >> 8,
                 mac2 & 0xff, mac2 >> 8);
    }
    if (features & VIRTIO_NET_F_GUEST_TSOV4) k_printf("tsov4g ");
    if (features & VIRTIO_NET_F_GUEST_TSOV6) k_printf("tsov6g ");
    if (features & VIRTIO_NET_F_GUEST_ECN) k_printf("tso/ecng ");
    if (features & VIRTIO_NET_F_GUEST_UFO) k_printf("ufog ");
    if (features & VIRTIO_NET_F_HOST_TSO4) k_printf("tsov4d ");
    if (features & VIRTIO_NET_F_MRG_RXBUF) k_printf("rbufmrg ");
    if (features & VIRTIO_NET_F_STATUS) {
        uint16_t sta;
        inw(portbase + 0x1a, sta);
        k_printf("sta=%x ", sta);
    }
    if (features & VIRTIO_NET_F_CTRL_VQ) k_printf("ctlvq ");
    if (features & VIRTIO_NET_F_CTRL_RX) k_printf("ctlrx ");
    if (features & VIRTIO_NET_F_CTRL_VLAN) k_printf("vlan ");
    if (features & VIRTIO_NET_F_GUEST_ANNOUNCE) k_printf("announceg ");
    k_printf("]\n");

    return 0;
}
