 

#ifndef ICE_NET_H
#define ICE_NET_H

#include "../types.h"

 
typedef struct {
    u8 addr[6];
} mac_addr_t;

 
typedef u32 ipv4_addr_t;

#define IP(a,b,c,d) (((u32)(a)<<24)|((u32)(b)<<16)|((u32)(c)<<8)|(u32)(d))

 
typedef struct {
    char name[8];
    mac_addr_t mac;
    ipv4_addr_t ip;
    ipv4_addr_t netmask;
    ipv4_addr_t gateway;
    bool up;
    bool link;
} net_iface_t;

 
typedef struct {
    u32 rx_packets;
    u32 tx_packets;
    u32 rx_bytes;
    u32 tx_bytes;
    u32 rx_errors;
    u32 tx_errors;
} net_stats_t;

 
int net_init(void);

 
net_iface_t* net_get_iface(int index);

 
int net_set_ip(int iface, ipv4_addr_t ip, ipv4_addr_t netmask);
int net_set_gateway(ipv4_addr_t gateway);

 
int net_send(int iface, const void *data, u32 len);
int net_recv(int iface, void *data, u32 maxlen);

 
bool net_is_available(void);

 
int net_arp_resolve(ipv4_addr_t ip, mac_addr_t *mac);

// Ping a host (returns RTT in ms, negative on error)
int net_ping(ipv4_addr_t dst, int timeout_ms);

// Get network statistics
net_stats_t* net_get_stats(void);

// IP address utilities
void net_ip_to_str(ipv4_addr_t ip, char *str);
ipv4_addr_t net_str_to_ip(const char *str);

#endif  
