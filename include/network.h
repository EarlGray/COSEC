#ifndef __COSEC_NETWORK_H__
#define __COSEC_NETWORK_H__

#include <stddef.h>
#include <stdint.h>

/*
 *    Network byte order
 */

static inline uint16_t htons(uint16_t hostshort) {
    return (hostshort >> 8) | (hostshort << 8);
}

static inline uint16_t ntohs(uint16_t hostshort) {
    return (hostshort >> 8) | (hostshort << 8);
}

static inline uint32_t htonl(uint32_t hostlong) {
    return ((uint32_t)htons(hostlong & 0xffff) << 16) | (uint32_t)(htons(hostlong >> 16));
}


/*
 *    Ethernet
 */

#define ETH_MTU     1500
#define ETH_ALEN    6

typedef union {
    uint8_t oct[ETH_ALEN];
    uint16_t word[ETH_ALEN/2];
} macaddr_t;

extern const macaddr_t ETH_BROADCAST_MAC;

enum ethertype {
    ETHERTYPE_IPV4 = 0x0800,
    ETHERTYPE_ARP = 0x0806,
    ETHERTYPE_IPV6 = 0x86DD,
};

__packed struct eth_hdr_t {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype;
};


#define IPV4_DEFAULT_TTL    32

union ipv4_addr_t {
    uint8_t oct[4];
    uint16_t word[2];
    uint32_t num;
};

__packed struct ipv4_hdr_t {
    uint8_t nwords:4;       // usually 5
    uint8_t version:4;      // 4 for IPv4
    uint8_t qos;
    uint16_t iplen;         // including IPv4 header
    uint16_t ident;
    uint16_t flags;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    union ipv4_addr_t src;
    union ipv4_addr_t dst;
};

enum ipv4_subproto {
    IPV4TYPE_ICMP = 1,
    IPV4TYPE_UDP = 17,
};

/*
 *  UDP
 */

__packed struct udp_hdr_t {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t udp_len;       // including UDP header
    uint16_t checksum;
};

uint8_t * net_buf_udp4_init(uint8_t *frame, size_t datalen);
void net_buf_udp4_setsrc(uint8_t *packet, const union ipv4_addr_t *addr, uint16_t port);
void net_buf_udp4_setdst(uint8_t *packet, const union ipv4_addr_t *addr, uint16_t port);
void net_buf_udp4_checksum(uint8_t *packet);
size_t net_buf_udp4_iplen(uint8_t *packet);


size_t net_buf_dhcpdiscover(uint8_t *frame, const macaddr_t *mac);

#endif  // __COSEC_NETWORK_H__
