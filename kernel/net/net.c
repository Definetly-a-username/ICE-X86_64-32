/*
 * ICE Operating System - Network Driver
 * Basic network stack (RTL8139 / E1000 / virtio-net)
 */

#include "net.h"
#include "../drivers/vga.h"

/* PCI ports for network detection */
#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

/* Network state */
static bool net_available = false;
static net_iface_t iface0 = {
    .name = "eth0",
    .mac = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    .ip = 0,
    .netmask = 0,
    .gateway = 0,
    .up = false,
    .link = false
};

static net_stats_t stats = {0};

/* I/O helpers */
static inline void outl(u16 port, u32 value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline u32 inl(u16 port) {
    u32 ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* PCI read config */
static u32 pci_read(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 addr = (1u << 31) | ((u32)bus << 16) | ((u32)slot << 11) |
               ((u32)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

/* Scan for network cards */
static bool net_detect_card(void) {
    /* Scan PCI bus for known network cards */
    for (u8 bus = 0; bus < 8; bus++) {
        for (u8 slot = 0; slot < 32; slot++) {
            u32 vendordev = pci_read(bus, slot, 0, 0);
            u16 vendor = vendordev & 0xFFFF;
            u16 device = vendordev >> 16;
            
            if (vendor == 0xFFFF) continue;
            
            /* Check for known network cards */
            /* RTL8139 */
            if (vendor == 0x10EC && device == 0x8139) {
                vga_puts("[NET] Found RTL8139\n");
                return true;
            }
            /* Intel E1000 */
            if (vendor == 0x8086 && (device == 0x100E || device == 0x100F)) {
                vga_puts("[NET] Found Intel E1000\n");
                return true;
            }
            /* Virtio-net */
            if (vendor == 0x1AF4 && device == 0x1000) {
                vga_puts("[NET] Found Virtio-net\n");
                return true;
            }
        }
    }
    return false;
}

int net_init(void) {
    /* Try to detect a network card */
    if (net_detect_card()) {
        net_available = true;
        iface0.up = false;
        iface0.link = false;
        
        /* Set dummy MAC for now */
        iface0.mac.addr[0] = 0x52;
        iface0.mac.addr[1] = 0x54;
        iface0.mac.addr[2] = 0x00;
        iface0.mac.addr[3] = 0x12;
        iface0.mac.addr[4] = 0x34;
        iface0.mac.addr[5] = 0x56;
        
        return 0;
    }
    
    net_available = false;
    return -1;
}

net_iface_t* net_get_iface(int index) {
    if (index == 0) return &iface0;
    return 0;
}

int net_set_ip(int iface, ipv4_addr_t ip, ipv4_addr_t netmask) {
    if (iface != 0) return -1;
    
    iface0.ip = ip;
    iface0.netmask = netmask;
    iface0.up = true;
    
    return 0;
}

int net_set_gateway(ipv4_addr_t gateway) {
    iface0.gateway = gateway;
    return 0;
}

int net_send(int iface, const void *data, u32 len) {
    (void)iface;
    (void)data;
    (void)len;
    
    if (!net_available) return -1;
    
    /* TODO: Implement actual packet sending */
    stats.tx_packets++;
    stats.tx_bytes += len;
    
    return len;
}

int net_recv(int iface, void *data, u32 maxlen) {
    (void)iface;
    (void)data;
    (void)maxlen;
    
    if (!net_available) return -1;
    
    /* TODO: Implement actual packet receiving */
    return 0;
}

bool net_is_available(void) {
    return net_available;
}

int net_arp_resolve(ipv4_addr_t ip, mac_addr_t *mac) {
    (void)ip;
    (void)mac;
    
    /* TODO: Implement ARP */
    return -1;
}

int net_ping(ipv4_addr_t dst, int timeout_ms) {
    (void)dst;
    (void)timeout_ms;
    
    if (!net_available) return -1;
    
    /* TODO: Implement ICMP ping */
    return -1;
}
