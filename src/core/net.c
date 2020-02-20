#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/errno.h>

#include <cosec/log.h>

#include <attrs.h>
#include "network.h"
#include "mem/pmem.h"

#include "arch/i386.h" // for cpu_halt

struct network_stack {
    struct netbuf *rxq;     // a circular double-linked list
    struct netiface * iface[MAX_NETWORK_INTERFACES];
};

struct network_stack theNetwork;


/*
 *  Utils
 */
static inline uint16_t ones_complement(uint16_t a, uint16_t b) {
    uint32_t s = (uint32_t)a;
    s += (uint32_t)b;
    return (uint16_t)((s & 0xffff) + (s >> 16));

}

static uint16_t ones_complement_words_sum(uint8_t *data, size_t len) {
    uint16_t *word = (uint16_t *)data;
    uint32_t s = 0;
    for (size_t i = 0; i < len/2; ++i) {
        s = ones_complement(s, ntohs(word[i]));
    }
    if (len%2) {
        s = ones_complement(s, (uint16_t)data[len-1] << 8);
    }
    return (uint16_t)(s & 0xffff);
}

/*
 *  Network interfaces
 */
int net_interface_register(struct netiface * iface) {
    for (int i = 0; i < MAX_NETWORK_INTERFACES; ++i) {
        if (theNetwork.iface[i] == NULL) {
            theNetwork.iface[i] = iface;
            iface->index = i;
            return 0;
        }
    }
    return ENODEV;
}

struct netiface * net_interface_for_destination(const union ipv4_addr_t *addr) {
    // TODO: routing
    return theNetwork.iface[0];
}

macaddr_t net_macaddr_for_ipv4(const union ipv4_addr_t addr) {
    // TODO: routing
    if ((addr.num == 0) || (addr.num == 0xffffffff))
        return ETH_BROADCAST_MAC;
    // TODO: arp and cache
    logmsgef("%s: TODO", __func__);
    return ETH_BROADCAST_MAC;
}

void net_macaddr_remember(const union ipv4_addr_t addr, const macaddr_t mac) {
    logmsgef("%s: TODO", __func__);
}


/*
 *  Receiving packets
 */

// To be called by a network driver.
// The driver is responsible for allocating the netbuf;
//   it also recycles it when `netbuf->recycle_frame()` is called by the network stack.
void net_receive_driver_frame(struct netbuf *qelem) {
    if (theNetwork.rxq) {
        struct netbuf *prev = theNetwork.rxq->prev;
        theNetwork.rxq->prev = qelem;
        qelem->next = theNetwork.rxq;
        prev->next = qelem;
        qelem->prev = prev;
    } else {
        theNetwork.rxq = qelem;
        qelem->prev = qelem->next = qelem;
    }
}

void net_wait_udp4(struct netbuf **nbuf, uint16_t port, uint32_t timeout_ms) {
    do {
        *nbuf = NULL;

        // walk the rxq looking for matching packet:
        if (theNetwork.rxq) {
            struct netbuf *cur = theNetwork.rxq;
            do {
                uint8_t *frame = cur->buf;
                struct eth_hdr_t *eth = (struct eth_hdr_t *)frame;
                if (ntohs(eth->ethertype) != ETHERTYPE_IPV4)
                    continue;

                struct ipv4_hdr_t *ip = (struct ipv4_hdr_t *)(frame + sizeof(struct eth_hdr_t));
                if (ip->proto != IPV4TYPE_UDP)
                    continue;

                struct udp_hdr_t *udp = (struct udp_hdr_t *)(frame + sizeof(struct eth_hdr_t) + 4*ip->nwords);
                uint16_t udp_dstport = ntohs(udp->dst_port);
                if ((port != 0) && (udp_dstport != port))
                    continue;

                // a match is found, remove it from the queue:
                // TODO: mutex
                if (theNetwork.rxq == cur) {
                    if (cur->next == cur) {
                        // the first and only element:
                        theNetwork.rxq = NULL;
                    } else {
                        // the first, but not the only element:
                        cur->next->prev = cur->prev;
                        cur->prev->next = cur->next;
                        theNetwork.rxq = cur->next;
                    }
                } else {
                    cur->next->prev = cur->prev;
                    cur->prev->next = cur->next;
                }

                *nbuf = cur;
                return;
            } while ((cur = cur->next) != theNetwork.rxq);
        }

        // TODO: timing out
        cpu_halt();
    } while (0);
}


/*
 *  Ethernet
 */
const macaddr_t ETH_BROADCAST_MAC = {
    .oct = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
};


/*
 *  UDP over IPv4 over Ethernet
 */

uint8_t * net_buf_udp4_alloc(const union ipv4_addr_t *srcaddr, uint16_t srcport,
                             const union ipv4_addr_t *dstaddr, uint16_t dstport)
{
    // Find the interface for srcaddr
    struct netiface *iface;
    iface = net_interface_for_destination(dstaddr);
    if (!iface) {
        logmsgdf("No interface for %d.%d.%d.%d", __func__,
                 srcaddr->oct[0], srcaddr->oct[1], srcaddr->oct[2], srcaddr->oct[3]);
        return NULL;
    }

    // Get mac addr for dstaddr
    macaddr_t srcmac = iface->get_mac();
    macaddr_t dstmac = net_macaddr_for_ipv4(*dstaddr);

    uint8_t *frame = iface->transmit_frame_alloc();
    if (!frame) return NULL;

    struct eth_hdr_t *eth = (struct eth_hdr_t *)frame;
    memcpy(eth->src, srcmac.oct, ETH_ALEN);
    memcpy(eth->dst, dstmac.oct, ETH_ALEN);
    eth->ethertype = htons(ETHERTYPE_IPV4);

    uint8_t *data = net_buf_udp4_init(frame);

    net_buf_udp4_setsrc(data, srcaddr, srcport);
    net_buf_udp4_setdst(data, dstaddr, dstport);

    return frame;
}

uint8_t * net_buf_udp4_init(uint8_t *frame) {
    // init ipv4
    struct ipv4_hdr_t *ipv4 = (struct ipv4_hdr_t*)(frame + sizeof(struct eth_hdr_t));
    ipv4->version = 4;
    ipv4->nwords = 5;
    ipv4->qos = 0;
    //ipv4->iplen = htons(sizeof(struct ipv4_hdr_t) + sizeof(struct udp_hdr_t) + datalen);
    ipv4->ident = 0;
    ipv4->flags = 0;
    ipv4->ttl = IPV4_DEFAULT_TTL;
    ipv4->proto = IPV4TYPE_UDP;
    ipv4->checksum = 0;

    return (uint8_t *)ipv4 + sizeof(struct ipv4_hdr_t) + sizeof(struct udp_hdr_t);
}

size_t net_buf_udp4_iplen(uint8_t *packet) {
    uint8_t *udpbuf = packet - sizeof(struct udp_hdr_t);
    struct udp_hdr_t *udp = (struct udp_hdr_t*)udpbuf;
    return sizeof(struct ipv4_hdr_t) + ntohs(udp->udp_len);
}

void net_buf_udp4_setsrc(uint8_t *data, const union ipv4_addr_t *addr, uint16_t port) {
    uint8_t *udpbuf = data - sizeof(struct udp_hdr_t);
    struct udp_hdr_t *udp = (struct udp_hdr_t*)udpbuf;
    struct ipv4_hdr_t *ipv4 = (struct ipv4_hdr_t*)(udpbuf - sizeof(struct ipv4_hdr_t));

    udp->src_port = htons(port);
    ipv4->src.num = addr->num;
}

void net_buf_udp4_setdst(uint8_t *data, const union ipv4_addr_t *addr, uint16_t port) {
    uint8_t *udpbuf = data - sizeof(struct udp_hdr_t);
    struct udp_hdr_t *udp = (struct udp_hdr_t*)udpbuf;
    struct ipv4_hdr_t *ipv4 = (struct ipv4_hdr_t*)(udpbuf - sizeof(struct ipv4_hdr_t));

    udp->dst_port = htons(port);
    ipv4->dst.num = addr->num;
}

size_t net_buf_udp4_checksum(uint8_t *data, size_t datalen) {
    uint8_t *udpbuf = data - sizeof(struct udp_hdr_t);

    struct udp_hdr_t *udp = (struct udp_hdr_t*)udpbuf;
    struct ipv4_hdr_t *ipv4 = (struct ipv4_hdr_t*)(udpbuf - sizeof(struct ipv4_hdr_t));

    udp->udp_len = htons(datalen + sizeof(struct udp_hdr_t));
    ipv4->iplen = htons(sizeof(struct ipv4_hdr_t) + sizeof(struct udp_hdr_t) + datalen);

    uint16_t checksum;

    // udp header+data checksum:
    /*
    checksum = ones_complement_words_sum(udpbuf, (size_t)ntohs(udp->udp_len));
    checksum = ones_complement(checksum, ntohs(ipv4->src.word[0]));
    checksum = ones_complement(checksum, ntohs(ipv4->src.word[1]));
    checksum = ones_complement(checksum, ntohs(ipv4->dst.word[0]));
    checksum = ones_complement(checksum, ntohs(ipv4->dst.word[1]));
    //checksum = ones_complement(checksum, IPV4TYPE_UDP);
    udp->checksum = htons(~checksum);
    */

    // ipv4 header checksum:
    checksum = ones_complement_words_sum((uint8_t*)ipv4, sizeof(struct ipv4_hdr_t));
    ipv4->checksum = htons(~checksum);

    return sizeof(struct eth_hdr_t) + sizeof(struct ipv4_hdr_t) + sizeof(struct udp_hdr_t) + datalen;
}

/*
 *  Local interface
 */

// TODO
struct netiface  local_iface = {
};


/*
 *  DHCP
 */

static void test_read_dhcpopts(uint8_t *opts) {
    while (opts[0] != DHCPOPT_END) {
        switch (opts[0]) {
        case DHCPOPT_OP:
            break;
        case DHCPOPT_SUBNET:
            logmsgif("  subnet:\t%d.%d.%d.%d", opts[2], opts[3], opts[4], opts[5]);
            break;
        case DHCPOPT_DNS:
            for (int i = 0; i < opts[1]/4; ++i) {
                logmsgif("  dns:\t%d.%d.%d.%d", opts[2 + 4*i], opts[3 + 4*i], opts[4 + 4*i], opts[5 + 4*i]);
            }
            break;
        case DHCPOPT_SRVADDR:
            logmsgif("  dhcpsrv:\t%d.%d.%d.%d", opts[2], opts[3], opts[4], opts[5]);
            break;
        case DHCPOPT_GW:
            logmsgif("  router:\t%d.%d.%d.%d", opts[2], opts[3], opts[4], opts[5]);
            break;
        case DHCPOPT_LEASETIME: {
            uint32_t t;
            for (int i = 0; i < opts[1]; ++i) t = 0x100 * t + (uint32_t)opts[2 + i];
            logmsgif("  lease:\t%d sec\n", t);
            } break;
        default:
            logmsgif("  opt 0x%x, len=%d", opts[0], opts[1]);
        }

        opts += (opts[1] + 2);
    }
}


void test_net_dhcp(void) {
    uint8_t *frame;
    size_t ethlen;

    struct eth_hdr_t *eth;
    struct ipv4_hdr_t *ip;
    struct dhcp4 *dhcp;
    uint8_t *data;
    const uint32_t xid = 0x3903f326;

    struct netbuf *reply;
    const uint32_t timeout_s = 300;

    struct netiface * iface = net_interface_for_destination(0);

    macaddr_t srvmac;

    // DHCP DISCOVER
    union ipv4_addr_t srcaddr = { .word = 0 };
    union ipv4_addr_t dstaddr = { .oct={255, 255, 255, 255} };

    frame = net_buf_udp4_alloc(&srcaddr, DHCP_CLIENT_PORT, &dstaddr, DHCP_SERVER_PORT);
    data = net_buf_udp4_init(frame);

    memset(data, 0, DHCP_OPT_OFFSET);
    dhcp = (struct dhcp4 *)data;
    dhcp->op = 1; dhcp->htype = 1; dhcp->hlen = ETH_ALEN;
    dhcp->xid = xid;
    memcpy(dhcp->chaddr, iface->get_mac().oct, ETH_ALEN);

    uint8_t *opts = data + DHCP_OPT_OFFSET;
    *(uint32_t*)(opts - sizeof(uint32_t)) = htonl(DHCP_MAGIC_COOKIE);

    uint8_t options[] = {
        DHCPOPT_OP, 1, DHCPOPT_OP_DISCOVERY,
        DHCPOPT_REQUEST, 4, DHCPOPT_SUBNET, DHCPOPT_DNS, DHCPOPT_GW, DHCPOPT_DOMAIN,
        DHCPOPT_END
    };
    memcpy(opts, options, sizeof(options));

    ethlen = net_buf_udp4_checksum(data, DHCP_OPT_OFFSET + sizeof(options));
    iface->transmit_frame_enqueue(frame, ethlen);
    iface->do_transmit();

    logmsgif("DHCP DISCOVER [xid=%x]", xid);

    // DHCP OFFER
    reply = NULL;
    net_wait_udp4(&reply, DHCP_CLIENT_PORT, timeout_s * 1000);
    if (!reply) {
        logmsgif("Timed out expecting the reply, %d sec", timeout_s);
        return;
    }
    logmsgf("%s: received *%x[%d]\n", __func__, reply->buf, reply->len);

    eth = (struct eth_hdr_t *)reply;
    memcpy(srvmac.oct, eth->src, ETH_ALEN);
    ip = (struct ipv4_hdr_t *)(reply->buf + sizeof(struct eth_hdr_t));
    dhcp = (struct dhcp4 *)((uint8_t *)ip + 4*ip->nwords + sizeof(struct udp_hdr_t));

    union ipv4_addr_t ipaddr = { .num = dhcp->yiaddr };
    union ipv4_addr_t server = { .num = dhcp->siaddr };
    logmsgif("DHCP OFFER [xid=0x%x]: siaddr=%d.%d.%d.%d, yiaddr=%d.%d.%d.%d", dhcp->xid,
             server.oct[0], server.oct[1], server.oct[2], server.oct[3],
             ipaddr.oct[0], ipaddr.oct[1], ipaddr.oct[2], ipaddr.oct[3]
    );
    //test_read_dhcpopts((uint8_t *)dhcp + DHCP_OPT_OFFSET);

    if (reply->recycle) reply->recycle(reply);

    // DHCP REQUEST
    frame = net_buf_udp4_alloc(&srcaddr, DHCP_CLIENT_PORT, &server, DHCP_SERVER_PORT);
    eth = (struct eth_hdr_t *)frame;
    memcpy(eth->dst, srvmac.oct, ETH_ALEN);

    data = net_buf_udp4_init(frame);
    memset(data, 0, DHCP_OPT_OFFSET);
    dhcp = (struct dhcp4 *)data;
    dhcp->op = 1; dhcp->htype = 1; dhcp->hlen = ETH_ALEN;
    dhcp->xid = xid;
    dhcp->siaddr = server.num;
    memcpy(dhcp->chaddr, iface->get_mac().oct, ETH_ALEN);

    *(uint32_t*)(data + DHCP_OPT_OFFSET - sizeof(uint32_t)) = htonl(DHCP_MAGIC_COOKIE);
    uint8_t reqopts[] = {
        DHCPOPT_OP, 1, DHCPOPT_OP_REQUEST,
        DHCPOPT_REQADDR, 4, ipaddr.oct[0], ipaddr.oct[1], ipaddr.oct[2], ipaddr.oct[3],
        DHCPOPT_SRVADDR, 4, server.oct[0], server.oct[1], server.oct[2], server.oct[3],
        DHCPOPT_END,
    };
    memcpy(data + DHCP_OPT_OFFSET, reqopts, sizeof(reqopts));

    ethlen = net_buf_udp4_checksum(data, DHCP_OPT_OFFSET + sizeof(reqopts));
    iface->transmit_frame_enqueue(frame, ethlen);
    iface->do_transmit();

    logmsgif("DHCP REQUEST [xid=%x] server=%d.%d.%d.%d ipaddr=%d.%d.%d.%d", xid,
             server.oct[0], server.oct[1], server.oct[2], server.oct[3],
             ipaddr.oct[0], ipaddr.oct[1], ipaddr.oct[2], ipaddr.oct[3]);

    // DHCP ACK
    reply = NULL;
    net_wait_udp4(&reply, DHCP_CLIENT_PORT, timeout_s * 1000);
    if (!reply) {
        logmsgif("Timed out expecting the reply, %d sec", timeout_s);
        return;
    }
    logmsgf("%s: received *%x[%d]\n", __func__, reply->buf, reply->len);

    eth = (struct eth_hdr_t *)reply;
    ip = (struct ipv4_hdr_t *)(reply->buf + sizeof(struct eth_hdr_t));
    dhcp = (struct dhcp4 *)((uint8_t *)ip + 4*ip->nwords + sizeof(struct udp_hdr_t));

    ipaddr.num = dhcp->yiaddr;
    server.num = dhcp->siaddr;
    logmsgif("DHCP ACK [xid=0x%x]: siaddr=%d.%d.%d.%d, yiaddr=%d.%d.%d.%d", dhcp->xid,
             server.oct[0], server.oct[1], server.oct[2], server.oct[3],
             ipaddr.oct[0], ipaddr.oct[1], ipaddr.oct[2], ipaddr.oct[3]
    );
    test_read_dhcpopts((uint8_t *)dhcp + DHCP_OPT_OFFSET);

    if (reply->recycle) reply->recycle(reply);
}
