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


