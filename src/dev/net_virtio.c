#include <string.h>
#include <sys/errno.h>

#include <attrs.h>
#include <network.h>
#include <mem/kheap.h>
#include <mem/pmem.h>
#include <dev/intrs.h>
#include <dev/pci.h>
#include <arch/i386.h>

#define __DEBUG
#include <cosec/log.h>
#include <algo.h>

#define VIRTIO_HW_CHECKSUM    0

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

/* VIO_DEV_STA register */
enum vio_dev_sta {
    STA_ACK           = 0x01,
    STA_DRV           = 0x02,
    STA_DRV_OK        = 0x04,
    STA_FAILED        = 0x80,
};

/* VIO_Dxx_FEATURE register */
enum vio_features {
    /* notify on empty avail_ring even if supppressed */
    VIRTIO_F_NOTIFY_ON_EMPTY  = (1u << 24),
    /* enable VIRTQ_DESC_F_INDIR */
    VIRTIO_F_RING_INDIR_DESC  = (1u << 28),
    /* enable `used_event` and `avail_event` */
    VIRTIO_F_RING_EVENT_IDX   = (1u << 29),
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

struct virtio_device {
    uint16_t    iobase;
    uint16_t    intr;
    uint32_t    features;
};

struct virtioq {
    uint16_t size;
    size_t npages;
    struct vring_desc   *desc;
    struct vring_avail  *avail;
    struct vring_used   *used;
};

static int virtio_vring_alloc(struct virtioq *q, uint16_t qsz) {
    const char *funcname = __FUNCTION__;

    size_t desctbl_sz = qsz * sizeof(struct vring_desc);
    size_t avail_sz = (3 + qsz) * sizeof(uint16_t);
    size_t used_sz = 3 * sizeof(uint16_t) + qsz * sizeof(struct vring_used_elem);

    size_t p = desctbl_sz + avail_sz;
    if (p % PAGE_SIZE) {
        p = ((p / PAGE_SIZE) + 1) * PAGE_SIZE;
    }
    size_t used_off = p;
    p += used_sz;
    if (p % PAGE_SIZE) {
        p = ((p / PAGE_SIZE) + 1) * PAGE_SIZE;
    }

    size_t npages = p / PAGE_SIZE;
    char *qmem = pmem_alloc(npages);
    return_err_if(!qmem, -ENOMEM,
                  "%s: pmem_alloc(%d) failed\n", funcname, npages);
    memset(qmem, 0, npages * PAGE_SIZE);
    q->size = qsz;
    q->npages = npages;
    q->desc = (struct vring_desc *)qmem;
    q->avail = (struct vring_avail *)(qmem + desctbl_sz);
    q->used = (struct vring_used *)(qmem + used_off);

    return 0;
}


/* VIO_Dxx_FEATURE for a network device */
enum vio_net_features {
    /* device handles packets with a partial checksum */
    VIRTIO_NET_F_CSUM       = (1u << 0),
    /* guest handles packets with a partial checksum */
    VIRTIO_NET_F_GUEST_CSUM = (1u << 1),
    /* device has given MAC address */
    VIRTIO_NET_F_MAC        = (1u << 5),
    VIRTIO_NET_F_GSO        = (1u << 6),
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

/* VIO_NET_STA register */
enum vio_net_sta {
    VIRTIO_NET_S_LINK_UP    = 1,
    VIRTIO_NET_S_ANNOUNCE   = 2,
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
    // uint16_t num_buffers; /* if VIRTIO_NET_F_MRG_RXBUF */
};


struct virtio_net_device {
    struct virtio_device virtio;
    macaddr_t   mac;

    struct virtioq  rxq;
    struct virtioq  txq;

    void *netbuf;
    size_t netbuf_npages;
};

struct virtio_net_device *theVirtNIC = NULL;

void net_virtio_irq() {
    const char *funcname = __FUNCTION__;

    uint8_t val;
    inb(theVirtNIC->virtio.iobase + VIO_ISR_STA, val);
    logmsge("%s: isr=0x%x\n", funcname, val);
}


uint8_t * net_virtio_frame_alloc(void) {
    uint8_t *buf = pmem_alloc(1);
    if (!buf) return NULL;

    logmsgdf("%s: buf at *%x\n", __func__, buf);

    return buf + sizeof(struct virtio_net_hdr);
}


void net_virtio_tx_enqueue(uint8_t *buf, size_t eth_payload_len) {
    logmsgdf("%s(buf=*%x, len=%d)\n", __func__, buf, eth_payload_len);
    /* virtio net header */
    struct virtio_net_hdr *vhdr = (struct virtio_net_hdr *)buf - 1;

#if (VIRTIO_HW_CHECKSUM)
    // TODO
    vhdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM; // TODO
    vhdr->csum_start = 0;
    vhdr->csum_offset = sizeof(struct eth_hdr_t) + eth_payload_len;
#endif

    /* remember it in txq */
    const size_t desc_offset = 0;
    struct vring_desc *desc = theVirtNIC->txq.desc + desc_offset;
    desc->addr = (uint64_t)(uint32_t)vhdr;
    desc->len = sizeof(struct virtio_net_hdr) + sizeof(struct eth_hdr_t) + eth_payload_len + 4;
    desc->flags = 0;
}

int net_virtio_transmit(void) {
    const size_t buf_idx = 0;
    struct virtioq *txq = &theVirtNIC->txq;

#if (!VIRTIO_HW_CHECKSUM)
    /* ethernet crc32 */
    struct vring_desc *desc = theVirtNIC->txq.desc + buf_idx;
    uint8_t *buf = (uint8_t *)((size_t)desc->addr + sizeof(struct virtio_net_hdr));

    uint8_t *p = buf;
    size_t len = desc->len - sizeof(struct virtio_net_hdr) - 4;
    uint32_t crc = ethernet_crc32(p, len);
    *(uint32_t*)(p + len) = crc;
#endif

    txq->avail->ring[txq->avail->idx % txq->size] = buf_idx;
    txq->avail->idx += 1;
        txq->avail->idx %= txq->size;

    /* notify the NIC */
    uint16_t val = VIRTIO_NET_TXQ;
    outw(theVirtNIC->virtio.iobase + VIO_Q_NOTIFY, val);

    return 0;
}

static int net_virtio_setup(struct virtio_net_device *nic) {
    const char *funcname = __FUNCTION__;

    int i, ret = 0;
    uint16_t port, hval;
    uint32_t val;

    hval = STA_ACK | STA_DRV;
    outw(nic->virtio.iobase + VIO_DEV_STA, hval);

    /* tx queue */
    hval = VIRTIO_NET_TXQ;
    outw(nic->virtio.iobase + VIO_Q_SELECT, hval);
    inw(nic->virtio.iobase + VIO_Q_SIZE, hval);
    assert(hval > 0, -ENOSYS, "%s: TX queue size is 0", funcname);

    ret = virtio_vring_alloc(&nic->txq, hval);
    return_msg_if(ret, ret, "%s: txq setup failed(%d)\n", funcname, ret);

    //nic->txq.avail->flags |= VIRTQ_AVAIL_F_NOINTR;

    logmsgdf("%s: tx_q[%d] at *%x (%d pages)\n", funcname,
             (int)hval, nic->txq.desc, nic->txq.npages);

    val = (uint32_t)nic->txq.desc / PAGE_SIZE;
    outl(nic->virtio.iobase + VIO_Q_ADDR, val);

    /* rx queue */
    hval = VIRTIO_NET_RXQ;
    outw(nic->virtio.iobase + VIO_Q_SELECT, hval);
    inw(nic->virtio.iobase + VIO_Q_SIZE, hval);
    assert(hval > 0, -ENOSYS, "%s: RX queue size is 0", funcname);

    ret = virtio_vring_alloc(&nic->rxq, hval);
    return_msg_if(ret, ret, "%s: rxq setup failed(%d)\n", funcname, ret);

    logmsgdf("%s: rx_q[%d] at *%x (%d pages)\n",
             funcname, (int)hval, nic->rxq.desc, nic->rxq.npages);

    /* netbuf: fill rx queue */
    const uint16_t packetsz = 2048;
    const uint32_t netbufsz = nic->rxq.size * packetsz;
    size_t netbuf_pages = netbufsz / PAGE_SIZE;
    if (netbufsz % PAGE_SIZE)
        ++netbuf_pages;
    uint8_t *netbuf = pmem_alloc(netbuf_pages);
    return_err_if(!netbuf, -ENOMEM,
                  "%s: failed to allocate netbuf\n", funcname);

    nic->netbuf = netbuf;
    nic->netbuf_npages = netbuf_pages;
    logmsgdf("%s: netbuf at *%x (%d pages)\n", funcname, netbuf, netbuf_pages);

    struct vring_avail *rxavail = nic->rxq.avail;
    rxavail->flags = 0;  // we do need an interrupt after each packet
    rxavail->idx = 0;    // ?
    for (i = 0; i < nic->rxq.size - 1; ++i) {
        struct vring_desc *vrd = nic->rxq.desc + i;
        vrd->addr = (uint64_t)(uint32_t)(netbuf + packetsz * i);
        vrd->len = packetsz;
        vrd->flags = VIRTQ_DESC_F_WRITE;
        vrd->next = 0;
        /* write this descriptor index into vring_avail */
        rxavail->ring[i] = (uint16_t)i;
        rxavail->idx += 1;
    }

    val = (uint32_t)nic->rxq.desc / PAGE_SIZE;
    outl(nic->virtio.iobase + VIO_Q_ADDR, val);

    /* negotiate features */
    val = VIRTIO_F_NOTIFY_ON_EMPTY;
    val |= VIRTIO_NET_F_MAC;
    val |= VIRTIO_NET_F_STATUS;
    val |= VIRTIO_NET_F_CSUM;
    outl(nic->virtio.iobase + VIO_DRV_FEATURE, val);
    logmsgdf("%s: negotiating 0x%x\n", funcname, val);

    inl(nic->virtio.iobase + VIO_DRV_FEATURE, val);
    logmsgdf("%s: negotiated  0x%x\n", funcname, val);
    nic->virtio.features = val;

    /* enable this virtio driver */
    hval = STA_DRV_OK | STA_DRV | STA_ACK;
    outw(nic->virtio.iobase + VIO_DEV_STA, hval);

    /* get network status */
    if (nic->virtio.features & VIRTIO_NET_F_STATUS) {
        inw(nic->virtio.iobase + VIO_NET_STA, hval);
        logmsgf("%s: virtio network status = 0x%x\n", funcname, hval);
    }

    /* setup IRQ */
    irq_set_handler(nic->virtio.intr, net_virtio_irq);
    irq_enable(nic->virtio.intr);
    logmsgdf("%s: irq_set_handler(%d, net_virtio_irq)\n",  funcname,
             nic->virtio.intr);

    return 0;
}

int net_virtio_init(pci_config_t *pciconf) {
    const char *funcname = __FUNCTION__;
    uint32_t features = 0;
    uint16_t portbase = 0;
    macaddr_t mac;

    return_msg_if(pciconf->pci_rev_id > 0, -ENOSYS,
                  "%s: pci.rev_id=%d, aborting configuration\n",
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
    if (features & VIRTIO_F_NOTIFY_ON_EMPTY) logmsgf("onempty ");
    if (features & VIRTIO_F_RING_INDIR_DESC) logmsgf("indir ");
    if (features & VIRTIO_NET_F_CSUM) logmsgf("csumd ");
    if (features & VIRTIO_NET_F_GUEST_CSUM) logmsgf("csumg ");
    if (features & VIRTIO_NET_F_GUEST_TSOV4) logmsgf("tsov4g ");
    if (features & VIRTIO_NET_F_GUEST_TSOV6) logmsgf("tsov6g ");
    if (features & VIRTIO_NET_F_GUEST_ECN) logmsgf("tso/ecng ");
    if (features & VIRTIO_NET_F_GUEST_UFO) logmsgf("ufog ");
    if (features & VIRTIO_NET_F_HOST_TSO4) logmsgf("tsov4d ");
    if (features & VIRTIO_NET_F_MRG_RXBUF) logmsgf("rxbufmrg ");
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

void net_virtio_macaddr(uint8_t addr[static ETH_ALEN]) {
    memcpy(addr, theVirtNIC->mac.oct, ETH_ALEN);
}

void test_virtio_net(void) {
    const char *echomsg = "Hello world!\n";
    size_t datalen = strlen(echomsg) + 1;

    uint8_t *frame = net_virtio_frame_alloc();

    struct eth_hdr_t *eth = (struct eth_hdr_t *)frame;
    memcpy(eth->src, theVirtNIC->mac.oct, ETH_ALEN);
    //memcpy(eth->dst, &ETH_BROADCAST_MAC, ETH_ALEN);
    const uint8_t mainland[] = {0x52,0x54,0x00,0x79,0x55,0x73};
    memcpy(eth->dst, mainland, ETH_ALEN);
    eth->ethertype = htons(ETHERTYPE_IPV4);

    uint8_t *data = net_buf_udp4_init(frame, datalen);
    strcpy((char *)data, echomsg);

    const union ipv4_addr_t srcaddr = { .oct={169,254,0,1} };
    const union ipv4_addr_t dstaddr = { .oct={192,168,122,1} };
    net_buf_udp4_setsrc(data, &srcaddr, 8000);
    net_buf_udp4_setdst(data, &dstaddr, 7777);
    net_buf_udp4_checksum(data);

    net_virtio_tx_enqueue(frame, net_buf_udp4_iplen(data));
    net_virtio_transmit();
}

void test_virtio_dhcp(void) {
    uint8_t *frame = net_virtio_frame_alloc();

    size_t ethlen = net_buf_dhcpdiscover(frame, &theVirtNIC->mac);

    net_virtio_tx_enqueue(frame, ethlen);
    net_virtio_transmit();
}
