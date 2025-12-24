/*
 * ICE Operating System - Network Driver Header
 * RTL8139 / NE2000 / E1000 compatible
 */

#ifndef ICE_NET_H
#define ICE_NET_H

#include "../types.h"

/* MAC address */
typedef struct {
    u8 addr[6];
} mac_addr_t;

/* IPv4 address */
typedef u32 ipv4_addr_t;

#define IP(a,b,c,d) (((u32)(a)<<24)|((u32)(b)<<16)|((u32)(c)<<8)|(u32)(d))

/* Network interface */
typedef struct {
    char name[8];
    mac_addr_t mac;
    ipv4_addr_t ip;
    ipv4_addr_t netmask;
    ipv4_addr_t gateway;
    bool up;
    bool link;
} net_iface_t;

/* Network statistics */
typedef struct {
    u32 rx_packets;
    u32 tx_packets;
    u32 rx_bytes;
    u32 tx_bytes;
    u32 rx_errors;
    u32 tx_errors;
} net_stats_t;

/* Initialize network subsystem */
int net_init(void);

/* Get interface */
net_iface_t* net_get_iface(int index);

/* Configure interface */
int net_set_ip(int iface, ipv4_addr_t ip, ipv4_addr_t netmask);
int net_set_gateway(ipv4_addr_t gateway);

/* Send/receive (raw) */
int net_send(int iface, const void *data, u32 len);
int net_recv(int iface, void *data, u32 maxlen);

/* Check if network is available */
bool net_is_available(void);

/* ARP */
int net_arp_resolve(ipv4_addr_t ip, mac_addr_t *mac);

/* ICMP ping */
int net_ping(ipv4_addr_t dst, int timeout_ms);

#endif /* ICE_NET_H */
