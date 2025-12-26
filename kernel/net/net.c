

#include "net.h"
#include "../drivers/vga.h"
#include "../drivers/pit.h"
#include "../tty/tty.h"

// PCI Configuration
#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// RTL8139 Registers
#define RTL_MAC0        0x00
#define RTL_MAR0        0x08
#define RTL_TXSTATUS0   0x10
#define RTL_TXADDR0     0x20
#define RTL_RXBUF       0x30
#define RTL_CMD         0x37
#define RTL_RXBUFTAIL   0x38
#define RTL_IMR         0x3C
#define RTL_ISR         0x3E
#define RTL_TCR         0x40
#define RTL_RCR         0x44
#define RTL_CONFIG1     0x52

// RTL8139 Command bits
#define RTL_CMD_RESET   0x10
#define RTL_CMD_RX_EN   0x08
#define RTL_CMD_TX_EN   0x04

// RTL8139 Config
#define RTL_RCR_AAP     0x01    // Accept all packets
#define RTL_RCR_APM     0x02    // Accept physical match
#define RTL_RCR_AM      0x04    // Accept multicast
#define RTL_RCR_AB      0x08    // Accept broadcast
#define RTL_RCR_WRAP    0x80    // Wrap mode

// Network state
static bool net_available = false;
static bool nic_initialized = false;
static u32 nic_io_base = 0;
static u8 nic_type = 0; // 0=none, 1=RTL8139, 2=E1000

static net_iface_t iface0 = {
    .name = "eth0",
    .mac = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}},
    .ip = 0,
    .netmask = 0,
    .gateway = 0,
    .up = false,
    .link = false
};

static net_stats_t stats = {0};

// RX/TX Buffers (must be page aligned in real implementation)
#define RX_BUF_SIZE 8192
#define TX_BUF_SIZE 4096
static u8 rx_buffer[RX_BUF_SIZE + 16] __attribute__((aligned(4096)));
static u8 tx_buffer[4][TX_BUF_SIZE] __attribute__((aligned(4096)));
static int current_tx = 0;
static u16 rx_index = 0;

// ARP Cache
#define ARP_CACHE_SIZE 16
typedef struct {
    ipv4_addr_t ip;
    mac_addr_t mac;
    u32 timestamp;
    bool valid;
} arp_entry_t;
static arp_entry_t arp_cache[ARP_CACHE_SIZE];

// Port I/O
static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline u16 inw(u16 port) {
    u16 ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(u16 port, u32 value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline u32 inl(u16 port) {
    u32 ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// PCI functions
static u32 pci_read(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 addr = (1u << 31) | ((u32)bus << 16) | ((u32)slot << 11) |
               ((u32)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write(u8 bus, u8 slot, u8 func, u8 offset, u32 value) {
    u32 addr = (1u << 31) | ((u32)bus << 16) | ((u32)slot << 11) |
               ((u32)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, value);
}

// Enable PCI bus mastering
static void pci_enable_bus_master(u8 bus, u8 slot, u8 func) {
    u32 cmd = pci_read(bus, slot, func, 0x04);
    cmd |= 0x07; // I/O space, memory space, bus master
    pci_write(bus, slot, func, 0x04, cmd);
}

// RTL8139 Driver
static void rtl8139_reset(void) {
    if (!nic_io_base) return;
    
    // Software reset
    outb(nic_io_base + RTL_CMD, RTL_CMD_RESET);
    
    // Wait for reset to complete
    for (int i = 0; i < 1000; i++) {
        if (!(inb(nic_io_base + RTL_CMD) & RTL_CMD_RESET)) break;
        pit_sleep_ms(1);
    }
}

static void rtl8139_read_mac(void) {
    if (!nic_io_base) return;
    
    for (int i = 0; i < 6; i++) {
        iface0.mac.addr[i] = inb(nic_io_base + RTL_MAC0 + i);
    }
}

static int rtl8139_init(u8 bus, u8 slot) {
    // Get I/O base address from BAR0
    u32 bar0 = pci_read(bus, slot, 0, 0x10);
    if (bar0 & 1) {
        nic_io_base = bar0 & 0xFFFC;
    } else {
        vga_puts("[NET] RTL8139: No I/O port found\n");
        return -1;
    }
    
    // Enable bus mastering
    pci_enable_bus_master(bus, slot, 0);
    
    // Power on
    outb(nic_io_base + RTL_CONFIG1, 0x00);
    
    // Reset
    rtl8139_reset();
    
    // Read MAC address
    rtl8139_read_mac();
    
    // Setup RX buffer
    outl(nic_io_base + RTL_RXBUF, (u32)rx_buffer);
    
    // Configure RX: accept broadcast + matching + multicast
    outl(nic_io_base + RTL_RCR, RTL_RCR_AB | RTL_RCR_AM | RTL_RCR_APM | RTL_RCR_WRAP);
    
    // Configure TX
    outl(nic_io_base + RTL_TCR, 0x03000700);
    
    // Enable RX and TX
    outb(nic_io_base + RTL_CMD, RTL_CMD_RX_EN | RTL_CMD_TX_EN);
    
    // Enable interrupts (optional - we use polling)
    outw(nic_io_base + RTL_IMR, 0x0005);
    
    vga_printf("[NET] RTL8139 initialized at I/O 0x%X\n", nic_io_base);
    vga_printf("[NET] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        iface0.mac.addr[0], iface0.mac.addr[1], iface0.mac.addr[2],
        iface0.mac.addr[3], iface0.mac.addr[4], iface0.mac.addr[5]);
    
    nic_initialized = true;
    nic_type = 1;
    return 0;
}

// Detect and initialize network card
static bool net_detect_and_init(void) {
    for (u8 bus = 0; bus < 8; bus++) {
        for (u8 slot = 0; slot < 32; slot++) {
            u32 vendordev = pci_read(bus, slot, 0, 0);
            u16 vendor = vendordev & 0xFFFF;
            u16 device = vendordev >> 16;
            
            if (vendor == 0xFFFF) continue;
            
            // RTL8139
            if (vendor == 0x10EC && device == 0x8139) {
                vga_puts("[NET] Found RTL8139\n");
                if (rtl8139_init(bus, slot) == 0) {
                    return true;
                }
            }
            
            // Intel E1000 (basic detection, driver not fully implemented)
            if (vendor == 0x8086 && (device == 0x100E || device == 0x100F)) {
                vga_puts("[NET] Found Intel E1000 (using default config)\n");
                // Set placeholder MAC
                iface0.mac.addr[0] = 0x52;
                iface0.mac.addr[1] = 0x54;
                iface0.mac.addr[2] = 0x00;
                iface0.mac.addr[3] = 0x12;
                iface0.mac.addr[4] = 0x34;
                iface0.mac.addr[5] = 0x56;
                nic_type = 2;
                return true;
            }
            
            // Virtio-net
            if (vendor == 0x1AF4 && device == 0x1000) {
                vga_puts("[NET] Found Virtio-net (using default config)\n");
                iface0.mac.addr[0] = 0x52;
                iface0.mac.addr[1] = 0x54;
                iface0.mac.addr[2] = 0x00;
                iface0.mac.addr[3] = 0x12;
                iface0.mac.addr[4] = 0x34;
                iface0.mac.addr[5] = 0x57;
                nic_type = 3;
                return true;
            }
        }
    }
    return false;
}

// Ethernet frame structure
typedef struct __attribute__((packed)) {
    u8 dst_mac[6];
    u8 src_mac[6];
    u16 ethertype;
} eth_header_t;

// ARP packet structure
typedef struct __attribute__((packed)) {
    u16 hw_type;
    u16 proto_type;
    u8 hw_len;
    u8 proto_len;
    u16 operation;
    u8 sender_mac[6];
    u32 sender_ip;
    u8 target_mac[6];
    u32 target_ip;
} arp_packet_t;

// IP header structure
typedef struct __attribute__((packed)) {
    u8 version_ihl;
    u8 tos;
    u16 total_len;
    u16 id;
    u16 flags_frag;
    u8 ttl;
    u8 protocol;
    u16 checksum;
    u32 src_ip;
    u32 dst_ip;
} ip_header_t;

// ICMP header structure
typedef struct __attribute__((packed)) {
    u8 type;
    u8 code;
    u16 checksum;
    u16 id;
    u16 sequence;
} icmp_header_t;

// Checksum calculation
static u16 calculate_checksum(void *data, int len) {
    u32 sum = 0;
    u16 *ptr = (u16*)data;
    
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(u8*)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

// Byte swap helpers
static inline u16 htons(u16 x) {
    return ((x & 0xFF) << 8) | ((x >> 8) & 0xFF);
}

static inline u32 htonl(u32 x) {
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) |
           ((x >> 8) & 0xFF00) | ((x >> 24) & 0xFF);
}

static inline u16 ntohs(u16 x) { return htons(x); }
static inline u32 ntohl(u32 x) { return htonl(x); }

// Memory copy
static void memcpy_net(void *dst, const void *src, u32 len) {
    u8 *d = dst;
    const u8 *s = src;
    while (len--) *d++ = *s++;
}

static void memset_net(void *dst, u8 val, u32 len) {
    u8 *d = dst;
    while (len--) *d++ = val;
}

// Send raw Ethernet frame
static int rtl8139_send(const void *data, u32 len) {
    if (!nic_initialized || len > TX_BUF_SIZE) return -1;
    
    // Copy to TX buffer
    memcpy_net(tx_buffer[current_tx], data, len);
    
    // Pad to minimum 60 bytes
    if (len < 60) {
        memset_net(tx_buffer[current_tx] + len, 0, 60 - len);
        len = 60;
    }
    
    // Set TX address and start transmission
    outl(nic_io_base + RTL_TXADDR0 + (current_tx * 4), (u32)tx_buffer[current_tx]);
    outl(nic_io_base + RTL_TXSTATUS0 + (current_tx * 4), len);
    
    // Wait for transmission (with timeout)
    for (int i = 0; i < 1000; i++) {
        u32 status = inl(nic_io_base + RTL_TXSTATUS0 + (current_tx * 4));
        if (status & 0x8000) { // TOK - Transmit OK
            stats.tx_packets++;
            stats.tx_bytes += len;
            current_tx = (current_tx + 1) % 4;
            return len;
        }
        if (status & 0x4000) { // TUN - Transmit FIFO underrun
            stats.tx_errors++;
            current_tx = (current_tx + 1) % 4;
            return -1;
        }
        pit_sleep_ms(1);
    }
    
    current_tx = (current_tx + 1) % 4;
    return -1;
}

// Receive packet (polling mode)
static int rtl8139_recv(void *buffer, u32 maxlen) {
    if (!nic_initialized) return -1;
    
    // Check if any packet received
    u8 cmd = inb(nic_io_base + RTL_CMD);
    if (cmd & 0x01) { // Buffer empty
        return 0;
    }
    
    // Get packet from RX buffer
    u16 *header = (u16*)(rx_buffer + rx_index);
    u16 status = header[0];
    u16 length = header[1];
    
    if (!(status & 0x01)) { // ROK bit not set
        return 0;
    }
    
    // Sanity check length
    if (length > maxlen || length > 1522) {
        stats.rx_errors++;
        return -1;
    }
    
    // Copy packet data (skip 4-byte header)
    memcpy_net(buffer, rx_buffer + rx_index + 4, length - 4);
    
    // Update RX index
    rx_index = (rx_index + length + 4 + 3) & ~3;
    if (rx_index >= RX_BUF_SIZE) rx_index -= RX_BUF_SIZE;
    outw(nic_io_base + RTL_RXBUFTAIL, rx_index - 16);
    
    stats.rx_packets++;
    stats.rx_bytes += length;
    
    return length - 4;
}

// ARP Functions
static void arp_cache_add(ipv4_addr_t ip, mac_addr_t *mac) {
    // Find empty or matching slot
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            slot = i;
            break;
        }
        if (arp_cache[i].ip == ip) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) slot = 0; // Overwrite first entry if full
    
    arp_cache[slot].ip = ip;
    memcpy_net(&arp_cache[slot].mac, mac, 6);
    arp_cache[slot].timestamp = (u32)pit_get_ticks();
    arp_cache[slot].valid = true;
}

static bool arp_cache_lookup(ipv4_addr_t ip, mac_addr_t *mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy_net(mac, &arp_cache[i].mac, 6);
            return true;
        }
    }
    return false;
}

static int arp_send_request(ipv4_addr_t target_ip) {
    u8 frame[60];
    eth_header_t *eth = (eth_header_t*)frame;
    arp_packet_t *arp = (arp_packet_t*)(frame + sizeof(eth_header_t));
    
    // Broadcast destination
    memset_net(eth->dst_mac, 0xFF, 6);
    memcpy_net(eth->src_mac, iface0.mac.addr, 6);
    eth->ethertype = htons(0x0806); // ARP
    
    // ARP request
    arp->hw_type = htons(1);        // Ethernet
    arp->proto_type = htons(0x0800); // IPv4
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->operation = htons(1);       // Request
    memcpy_net(arp->sender_mac, iface0.mac.addr, 6);
    arp->sender_ip = htonl(iface0.ip);
    memset_net(arp->target_mac, 0, 6);
    arp->target_ip = htonl(target_ip);
    
    return rtl8139_send(frame, sizeof(eth_header_t) + sizeof(arp_packet_t));
}

static void arp_process_reply(arp_packet_t *arp) {
    if (ntohs(arp->operation) == 2) { // ARP reply
        arp_cache_add(ntohl(arp->sender_ip), (mac_addr_t*)arp->sender_mac);
    }
}

// ICMP Functions
static int icmp_send_echo(ipv4_addr_t dst_ip, u16 id, u16 seq) {
    u8 frame[74]; // Ethernet + IP + ICMP + 8 bytes data
    
    eth_header_t *eth = (eth_header_t*)frame;
    ip_header_t *ip = (ip_header_t*)(frame + sizeof(eth_header_t));
    icmp_header_t *icmp = (icmp_header_t*)(frame + sizeof(eth_header_t) + sizeof(ip_header_t));
    
    // Get destination MAC
    mac_addr_t dst_mac;
    if (!arp_cache_lookup(dst_ip, &dst_mac)) {
        // Try gateway if not on local network
        if ((dst_ip & iface0.netmask) != (iface0.ip & iface0.netmask)) {
            if (iface0.gateway && arp_cache_lookup(iface0.gateway, &dst_mac)) {
                // Use gateway MAC
            } else {
                return -1; // No route
            }
        } else {
            return -1; // Not in ARP cache
        }
    }
    
    // Ethernet header
    memcpy_net(eth->dst_mac, dst_mac.addr, 6);
    memcpy_net(eth->src_mac, iface0.mac.addr, 6);
    eth->ethertype = htons(0x0800); // IPv4
    
    // IP header
    ip->version_ihl = 0x45; // IPv4, 5 words (20 bytes)
    ip->tos = 0;
    ip->total_len = htons(28); // IP header + ICMP header + 8 bytes data
    ip->id = htons(id);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = 1; // ICMP
    ip->checksum = 0;
    ip->src_ip = htonl(iface0.ip);
    ip->dst_ip = htonl(dst_ip);
    ip->checksum = calculate_checksum(ip, sizeof(ip_header_t));
    
    // ICMP header
    icmp->type = 8; // Echo request
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->sequence = htons(seq);
    
    // Data (timestamp or pattern)
    u8 *data = (u8*)(icmp + 1);
    for (int i = 0; i < 8; i++) {
        data[i] = i;
    }
    
    // ICMP checksum includes data
    icmp->checksum = calculate_checksum(icmp, 16);
    
    return rtl8139_send(frame, 74);
}

// Process received packet
static void process_packet(u8 *data, int len) {
    if (len < (int)sizeof(eth_header_t)) return;
    
    eth_header_t *eth = (eth_header_t*)data;
    u16 ethertype = ntohs(eth->ethertype);
    
    if (ethertype == 0x0806) { // ARP
        arp_packet_t *arp = (arp_packet_t*)(data + sizeof(eth_header_t));
        arp_process_reply(arp);
    } else if (ethertype == 0x0800) { // IPv4
        // Process IP packet (for ICMP responses, etc.)
    }
}

// Public API Implementation
int net_init(void) {
    // Initialize ARP cache
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache[i].valid = false;
    }
    
    // Detect and initialize NIC
    if (net_detect_and_init()) {
        net_available = true;
        iface0.up = false;
        iface0.link = (nic_type == 1 && nic_initialized);
        return 0;
    }
    
    vga_puts("[NET] No network card detected\n");
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
    
    if (!net_available) return -1;
    
    if (nic_type == 1 && nic_initialized) {
        return rtl8139_send(data, len);
    }
    
    // Simulated send for unsupported NICs
    stats.tx_packets++;
    stats.tx_bytes += len;
    return len;
}

int net_recv(int iface, void *data, u32 maxlen) {
    (void)iface;
    
    if (!net_available) return -1;
    
    if (nic_type == 1 && nic_initialized) {
        int len = rtl8139_recv(data, maxlen);
        if (len > 0) {
            process_packet(data, len);
        }
        return len;
    }
    
    return 0;
}

bool net_is_available(void) {
    return net_available;
}

int net_arp_resolve(ipv4_addr_t ip, mac_addr_t *mac) {
    // Check cache first
    if (arp_cache_lookup(ip, mac)) {
        return 0;
    }
    
    if (!nic_initialized) return -1;
    
    // Send ARP request
    for (int retry = 0; retry < 3; retry++) {
        arp_send_request(ip);
        
        // Wait for response
        for (int i = 0; i < 100; i++) { // 1 second timeout
            u8 buf[1600];
            int len = rtl8139_recv(buf, sizeof(buf));
            if (len > 0) {
                process_packet(buf, len);
                if (arp_cache_lookup(ip, mac)) {
                    return 0;
                }
            }
            pit_sleep_ms(10);
        }
    }
    
    return -1;
}

int net_ping(ipv4_addr_t dst, int timeout_ms) {
    if (!net_available || !iface0.up) return -1;
    
    static u16 ping_id = 1;
    static u16 ping_seq = 0;
    
    ping_id++;
    ping_seq++;
    
    // Resolve MAC address first
    mac_addr_t dst_mac;
    ipv4_addr_t resolve_ip = dst;
    
    // Use gateway for non-local addresses
    if ((dst & iface0.netmask) != (iface0.ip & iface0.netmask)) {
        resolve_ip = iface0.gateway;
    }
    
    if (!arp_cache_lookup(resolve_ip, &dst_mac)) {
        // Need to resolve
        if (net_arp_resolve(resolve_ip, &dst_mac) < 0) {
            return -2; // ARP failed
        }
    }
    
    // Send ICMP echo request
    u64 start_time = pit_get_ticks();
    
    if (icmp_send_echo(dst, ping_id, ping_seq) < 0) {
        return -3; // Send failed
    }
    
    // Wait for response
    u8 buf[1600];
    while (1) {
        u64 elapsed = (pit_get_ticks() - start_time) * 10; // Approx milliseconds
        if ((int)elapsed >= timeout_ms) {
            return -1; // Timeout
        }
        
        int len = rtl8139_recv(buf, sizeof(buf));
        if (len > 0) {
            eth_header_t *eth = (eth_header_t*)buf;
            if (ntohs(eth->ethertype) == 0x0800) { // IPv4
                ip_header_t *ip = (ip_header_t*)(buf + sizeof(eth_header_t));
                if (ip->protocol == 1 && ntohl(ip->src_ip) == dst) { // ICMP from target
                    icmp_header_t *icmp = (icmp_header_t*)(buf + sizeof(eth_header_t) + sizeof(ip_header_t));
                    if (icmp->type == 0 && ntohs(icmp->id) == ping_id) { // Echo reply
                        return (int)((pit_get_ticks() - start_time) * 10); // RTT in ms
                    }
                }
            }
        }
        
        pit_sleep_ms(1);
    }
}

// Get network statistics
net_stats_t* net_get_stats(void) {
    return &stats;
}

// Format IP address as string
void net_ip_to_str(ipv4_addr_t ip, char *str) {
    u8 *b = (u8*)&ip;
    // IP is in host order, need to print correctly
    int pos = 0;
    for (int i = 3; i >= 0; i--) {
        u8 octet = b[i];
        if (octet >= 100) {
            str[pos++] = '0' + (octet / 100);
            octet %= 100;
            str[pos++] = '0' + (octet / 10);
            str[pos++] = '0' + (octet % 10);
        } else if (octet >= 10) {
            str[pos++] = '0' + (octet / 10);
            str[pos++] = '0' + (octet % 10);
        } else {
            str[pos++] = '0' + octet;
        }
        if (i > 0) str[pos++] = '.';
    }
    str[pos] = 0;
}

// Parse IP address from string
ipv4_addr_t net_str_to_ip(const char *str) {
    u32 octets[4] = {0};
    int octet = 0;
    
    while (*str && octet < 4) {
        if (*str >= '0' && *str <= '9') {
            octets[octet] = octets[octet] * 10 + (*str - '0');
        } else if (*str == '.') {
            octet++;
        }
        str++;
    }
    
    return IP(octets[0], octets[1], octets[2], octets[3]);
}
