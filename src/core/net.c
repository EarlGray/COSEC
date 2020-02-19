#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <attrs.h>
#include "network.h"
#include "mem/pmem.h"

/*
 *  Ethernet
 */
const macaddr_t ETH_BROADCAST_MAC = {
    .oct = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
};

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
 *  UDP over IPv4 over Ethernet
 */

uint8_t * net_buf_udp4_init(uint8_t *frame, size_t datalen) {
    if (sizeof(struct ipv4_hdr_t) + sizeof(struct udp_hdr_t) + datalen > ETH_MTU) {
        return NULL;
    }

    uint8_t *udpbuf = frame + sizeof(struct eth_hdr_t) + sizeof(struct ipv4_hdr_t);

    struct udp_hdr_t *udp = (struct udp_hdr_t*)udpbuf;
    struct ipv4_hdr_t *ipv4 = (struct ipv4_hdr_t*)(udpbuf - sizeof(struct ipv4_hdr_t));

    // init udp
    udp->udp_len = htons(datalen + sizeof(struct udp_hdr_t));

    // init ipv4
    ipv4->version = 4;
    ipv4->nwords = 5;
    ipv4->qos = 0;
    ipv4->iplen = htons(sizeof(struct ipv4_hdr_t) + sizeof(struct udp_hdr_t) + datalen);
    ipv4->ident = 0;
    ipv4->flags = 0;
    ipv4->ttl = IPV4_DEFAULT_TTL;
    ipv4->proto = IPV4TYPE_UDP;
    ipv4->checksum = 0;

    return udpbuf + sizeof(struct udp_hdr_t);
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

void net_buf_udp4_checksum(uint8_t *data) {
    uint8_t *udpbuf = data - sizeof(struct udp_hdr_t);
    struct udp_hdr_t *udp = (struct udp_hdr_t*)udpbuf;
    struct ipv4_hdr_t *ipv4 = (struct ipv4_hdr_t*)(udpbuf - sizeof(struct ipv4_hdr_t));
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
}


/*
 *  DHCP
 */

#define DHCP_ZERO_PADDING   192
#define DHCP_MAGIC_COOKIE   0x63825363

#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68

__packed struct dhcp4 {
    uint8_t op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr;
    uint16_t chaddr[8];
};

const uint8_t dhcp_discovery_options[] = {
    0x35, 1, 1,                         // discovery op
    0x37, 4, 0x01, 0x03, 0x0f, 0x06,    // ask for subnet, router, domain, DNS server
    // 0x32, 4, 192, 168, 1, 101,
    0xff
};

size_t net_buf_dhcpdiscover(uint8_t *frame, const macaddr_t *mac) {
    const size_t options_offset = sizeof(struct dhcp4) + DHCP_ZERO_PADDING + sizeof(uint32_t);
    const size_t datalen = options_offset + sizeof(dhcp_discovery_options);

    struct eth_hdr_t *eth = (struct eth_hdr_t *)frame;
    memcpy(eth->src, mac->oct, ETH_ALEN);
    memcpy(eth->dst, &ETH_BROADCAST_MAC, ETH_ALEN);
    eth->ethertype = htons(ETHERTYPE_IPV4);

    uint8_t *data = net_buf_udp4_init(frame, datalen);
    memset(data, 0, datalen);
    struct dhcp4 *dhcp_hdr = (struct dhcp4 *)data;
    dhcp_hdr->op = 1;
    dhcp_hdr->htype = 1;
    dhcp_hdr->hlen = ETH_ALEN;
    dhcp_hdr->xid = htonl(0x3903f326);
    dhcp_hdr->chaddr[0] = mac->word[0]; dhcp_hdr->chaddr[1] = mac->word[1]; dhcp_hdr->chaddr[2] = mac->word[2];

    *(uint32_t*)(data + options_offset - sizeof(uint32_t)) = htonl(DHCP_MAGIC_COOKIE);
    memcpy(data + options_offset, dhcp_discovery_options, sizeof(dhcp_discovery_options));

    const union ipv4_addr_t srcaddr = { .oct={0,0,0,0} };
    const union ipv4_addr_t dstaddr = { .oct={255,255,255,255} };
    net_buf_udp4_setsrc(data, &srcaddr, DHCP_CLIENT_PORT);
    net_buf_udp4_setdst(data, &dstaddr, DHCP_SERVER_PORT);
    net_buf_udp4_checksum(data);

    return sizeof(struct ipv4_hdr_t) + sizeof(struct udp_hdr_t) + datalen;
}
