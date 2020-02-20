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
 *  receive
 */

struct netbuf {
    struct netbuf *next;
    struct netbuf *prev;

    uint8_t *buf;
    uint16_t len;

    void (*recycle)(struct netbuf *);
};

void net_receive_driver_frame(struct netbuf *qelem);

// Blocks waiting to receive a UDP4 datagram and consumes it.
// *nbuf will be set to the received frame or to NULL if timed out.
// If timeout_ms = 0, it does not block and returns immediately;
// port specified the UDP port; 0 means "any destination port".
// When done with nbuf, you must clean up: `nbuf->recycle(nbuf)`
void net_wait_udp4(struct netbuf **nbuf, uint16_t port, uint32_t timeout_ms);



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


/*
 *  DHCP
 */

#define DHCP_ZERO_PADDING   192
#define DHCP_MAGIC_COOKIE   0x63825363

#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68

enum dhcp_opt {
    DHCPOPT_REQADDR = 0x32,     // length 4
    DHCPOPT_LEASETIME = 0x33,   // length 4
    DHCPOPT_OP      = 0x35,     // length 1
    DHCPOPT_SRVADDR = 0x36,     // length 4
    DHCPOPT_REQUEST = 0x37,     // variable length
    DHCPOPT_END     = 0xff,
};

enum dhcpopt_op {
    DHCPOPT_OP_DISCOVERY  = 1,
    DHCPOPT_OP_OFFER      = 2,
    DHCPOPT_OP_REQUEST    = 3,
    DHCPOPT_OP_ACK        = 4,
};

enum dhcpopt_request {
    DHCPOPT_SUBNET  = 0x01,
    DHCPOPT_GW      = 0x03,
    DHCPOPT_DNS     = 0x06,
    DHCPOPT_DOMAIN  = 0x0f,
};

__packed struct dhcp4 {
    uint8_t op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr;
    uint16_t chaddr[8];
};

static const size_t DHCP_OPT_OFFSET = sizeof(struct dhcp4) + DHCP_ZERO_PADDING + sizeof(uint32_t);

/*
 *  transmit
 */

// Pre-initializes an ethernet frame with IPv4 with UDP
uint8_t * net_buf_udp4_alloc(const union ipv4_addr_t *srcaddr, uint16_t srcport,
                             const union ipv4_addr_t *dstaddr, uint16_t dstport);

// Takes an ethernet frame and returns the pointer to UDP data
uint8_t * net_buf_udp4_init(uint8_t *frame);

void net_buf_udp4_setsrc(uint8_t *packet, const union ipv4_addr_t *addr, uint16_t port);
void net_buf_udp4_setdst(uint8_t *packet, const union ipv4_addr_t *addr, uint16_t port);
size_t net_buf_udp4_iplen(uint8_t *packet);

// Does UDP and IP checksumming, initializes the length of the packet.
// Returns the frame length.
size_t net_buf_udp4_checksum(uint8_t *data, size_t datalen);


/*
 *  network driver interface
 */

// Yes, that's right, just one interface for now.
// I will need routing to have more.
#define MAX_NETWORK_INTERFACES  1

struct netiface {
    struct netiface *next;

    uint32_t index;

    macaddr_t (*get_mac)(void);

    uint8_t* (*transmit_frame_alloc)(void);
    void (*transmit_frame_enqueue)(uint8_t *, size_t);
    int (*do_transmit)(void);
};

int net_interface_register(struct netiface *);
struct netiface * net_interface_for_destination(const union ipv4_addr_t *);


#endif  // __COSEC_NETWORK_H__
