#include <string.h>
#include <sys/errno.h>

#include <attrs.h>
#include <mem/kheap.h>
#include <mem/pmem.h>
#include <dev/intrs.h>
#include <dev/pci.h>
#include <arch/i386.h>

#define __DEBUG
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

#define VIO_DEVIO_OFF      VIO_DEVICE_SPECIFIC_OFFSET

#define VIO_NET_MAC           (VIO_DEVIO_OFF + 0)   /* 48, R, if VIRTIO_NET_F_MAC */
#define VIO_NET_STA           (VIO_DEVIO_OFF + 6)   /* 16, R, if VIRTIO_NET_F_STATUS */

/* may be written into VIO_MSIX_CONF_VECT */
#define VIRTIO_MSI_NO_VECTOR  0xffff

#define VIRTIO_NET_RXQ  0
#define VIRTIO_NET_TXQ  1
#define VIRTIO_NET_CTLQ 2

#define VIRTIO_PAD  4096

/* VIO_Dxx_FEATURE register */
enum vio_features {
    /* notify on empty avail_ring even if supppressed */
    VIRTIO_F_NOTIFY_ON_EMPTY  = (1u << 24),
    /* enable VIRTQ_DESC_F_INDIR */
    VIRTIO_F_RING_INDIR_DESC  = (1u << 28),
    /* enable `used_event` and `avail_event` */
    VIRTIO_F_RING_EVENT_IDX   = (1u << 29),
};

/* VIO_Dxx_FEATURE for a network device */
enum vio_net_features {
    /* device handles packets with a partial checksum */
    VIRTIO_NET_F_CSUM       = (1u << 0),
    /* guest handles packets with a partial checksum */
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

/* VIO_NET_STA register */
enum vio_net_sta {
    VIRTIO_NET_S_LINK_UP    = 1,
    VIRTIO_NET_S_ANNOUNCE   = 2,
};

enum vring_desc_flags {
    VIRTQ_DESC_F_NEXT  = 1,
    VIRTQ_DESC_F_WRITE = 2,
    VIRTQ_DESC_F_INDIR = 4,
};

struct __packed vring_desc {
    uint64_t addr;
    uint32_t len;

    uint16_t flags; /* see: enum vring_desc_flags */
    uint16_t next;  /* if:  flags & VIRTQ_DESC_F_NEXT */
};

#define VIRTQ_AVAIL_F_NOINTR  1
struct __packed vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct __packed vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
};

struct virtioq {
    uint16_t size;
    size_t npages;
    struct vring_desc   *desc;
    struct vring_avail  *avail;
    struct vring_used   *used;
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

#define ETH_ALEN    6

typedef union {
    uint8_t oct[ETH_ALEN];
    uint16_t word[ETH_ALEN/2];
} macaddr_t;

struct virtio_device {
    uint16_t    iobase;
    int         intr;
    uint32_t    features;
};

struct virtio_net_device {
    struct virtio_device virtio;
    macaddr_t   mac;

    struct virtioq  rxq;
    struct virtioq  txq;
};


struct virtio_net_device *theVirtNIC = 0;

static int virtio_vring_alloc(struct virtioq *q, uint16_t qsz) {
    const char *funcname = __FUNCTION__;
    q->size = qsz;

    size_t desctbl_sz = sizeof(struct vring_desc) * qsz;
    size_t avail_sz = sizeof(uint16_t) * (3 + qsz);
    size_t used_sz = 6 + sizeof(struct vring_used_elem);

    size_t npages = desctbl_sz + avail_sz;
    if (npages % VIRTIO_PAD) {
        npages = ((npages >> 12) + 1) << 12;
    }
    size_t used_off = npages;
    npages += used_sz;
    if (npages % VIRTIO_PAD) {
        npages = ((npages >> 12) + 1) << 12;
    }
    npages = npages / PAGE_SIZE;

    char *qmem = pmem_alloc(npages);
    return_err_if(!qmem, -ENOMEM,
                  "%s: pmem_alloc(%d) failed\n", funcname, npages);
    memset(qmem, 0, npages * PAGE_SIZE);
    q->npages = npages;
    q->desc = (struct vring_desc *)qmem;
    q->avail = (struct vring_avail *)(qmem + avail_sz);
    q->used = (struct vring_used *)(qmem + used_off);

    return 0;
}

static void net_virtio_irq() {
    const char *funcname = __FUNCTION__;
    logmsgf("%s: tick\n", funcname);
}

static int net_virtio_setup(struct virtio_net_device *nic) {
    const char *funcname = __FUNCTION__;
    int ret = 0;
    uint16_t port, val;

    val = STA_ACK | STA_DRV;
    outw(nic->virtio.iobase + VIO_DEV_STA, val);

    /* rx queue */
    val = VIRTIO_NET_RXQ;
    outw(nic->virtio.iobase + VIO_Q_SELECT, val);
    inw(nic->virtio.iobase + VIO_Q_SIZE, val);
    ret = virtio_vring_alloc(&nic->rxq, val);
    return_msg_if(ret, ret,
                  "%s: rxq setup failed(%d)\n", funcname, ret);
    logmsgdf("%s: rx_q[%d] at *%x (%d pages)\n",
             funcname, (int)val, nic->rxq.desc, nic->rxq.npages);

    /* tx queue */
    val = VIRTIO_NET_TXQ;
    outw(nic->virtio.iobase + VIO_Q_SELECT, val);
    inw(nic->virtio.iobase + VIO_Q_SIZE, val);
    ret = virtio_vring_alloc(&nic->txq, val);
    return_msg_if(ret, ret,
                  "%s: txq setup failed(%s)\n", funcname, ret);
    logmsgdf("%s: tx_q[%d] at *%x (%d pages)\n", funcname,
             (int)val, nic->txq.desc, nic->txq.npages);

    /* negotiate features */
    uint32_t v32 = VIRTIO_NET_F_MAC;
    v32 |= VIRTIO_NET_F_STATUS;
    v32 |= VIRTIO_NET_F_CSUM;
    outw(nic->virtio.iobase + VIO_DRV_FEATURE, v32);

    /* setup IRQ */
    irq_set_handler(nic->virtio.intr, net_virtio_irq);
    irq_mask(nic->virtio.intr, true);

    /* enable this virtio driver */
    val |= STA_DRV_OK;
    outw(nic->virtio.iobase + VIO_DEV_STA, val);

    /* get network status */
    if (nic->virtio.features & VIRTIO_NET_F_STATUS) {
        inw(nic->virtio.iobase + VIO_NET_STA, val);
        logmsgf("%s: virtio network status = 0x%x\n", funcname, val);
    }

    return 0;
}

int net_virtio_init(pci_config_t *pciconf) {
    const char *funcname = __FUNCTION__;
    uint32_t features = 0;
    uint16_t portbase = 0;
    macaddr_t mac;

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

    inl(portbase + VIO_DEV_FEATURE, features);

    return_err_if(!(features & VIRTIO_NET_F_MAC), -EINVAL, 
                  "%s: no MAC address, aborting configuration", funcname);
    uint16_t mac0, mac1, mac2;
    uint16_t macp = portbase + VIO_NET_MAC;
    inw(macp + 0, mac0);
    inw(macp + 2, mac1);
    inw(macp + 4, mac2);
    mac.word[0] = mac0;
    mac.word[1] = mac1;
    mac.word[2] = mac2;
    logmsgif("%s: mac=%x:%x:%x:%x:%x:%x", funcname,
             mac.oct[0], mac.oct[1],
             mac.oct[2], mac.oct[3],
             mac.oct[4], mac.oct[5]);

    logmsgf("%s: portbase = 0x%x, intr = #%d\n", funcname,
             (uint)portbase, pciconf->pci_interrupt_line);

    logmsgf("%s: [", funcname);
    if (features & VIRTIO_NET_F_STATUS) {
        uint16_t sta;
        inw(portbase + VIO_NET_STA, sta);
        logmsgf("sta=%x ", sta);
    }
    if (features & VIRTIO_NET_F_CSUM) logmsgf("csumd ");
    if (features & VIRTIO_NET_F_GUEST_CSUM) logmsgf("csumg ");
    if (features & VIRTIO_NET_F_GUEST_TSOV4) logmsgf("tsov4g ");
    if (features & VIRTIO_NET_F_GUEST_TSOV6) logmsgf("tsov6g ");
    if (features & VIRTIO_NET_F_GUEST_ECN) logmsgf("tso/ecng ");
    if (features & VIRTIO_NET_F_GUEST_UFO) logmsgf("ufog ");
    if (features & VIRTIO_NET_F_HOST_TSO4) logmsgf("tsov4d ");
    if (features & VIRTIO_NET_F_MRG_RXBUF) logmsgf("rbufmrg ");
    if (features & VIRTIO_NET_F_CTRL_VQ) logmsgf("ctlvq ");
    if (features & VIRTIO_NET_F_CTRL_RX) logmsgf("ctlrx ");
    if (features & VIRTIO_NET_F_CTRL_VLAN) logmsgf("vlan ");
    if (features & VIRTIO_NET_F_GUEST_ANNOUNCE) logmsgf("announceg ");
    logmsgf("]\n");

    return_err_if(theVirtNIC, -ETODO,
                  "%s: only one network device at the moment\n", funcname);

    theVirtNIC = kmalloc(sizeof(struct virtio_net_device));
    theVirtNIC->virtio.iobase = portbase;
    theVirtNIC->virtio.intr = pciconf->pci_interrupt_line;
    theVirtNIC->virtio.features = features;
    theVirtNIC->mac = mac;

    return net_virtio_setup(theVirtNIC);
}
