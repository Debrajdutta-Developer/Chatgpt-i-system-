#ifndef KPF_ENGINE_H
#define KPF_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#define KPF_MAX_PACKET_SIZE 65535
#define KPF_DEFAULT_IFACE "lo"
#define KPF_ETH_ALEN 6

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

// Ethernet Header
struct kpf_ethhdr {
    uint8_t  h_dest[KPF_ETH_ALEN];
    uint8_t  h_source[KPF_ETH_ALEN];
    uint16_t h_proto;
} __attribute__((packed));

// IPv4 Header
struct kpf_iphdr {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t ihl:4;
    uint8_t version:4;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t version:4;
    uint8_t ihl:4;
#else
#error "Please fix <bytesex.h>"
#endif
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
} __attribute__((packed));

// TCP Header
struct kpf_tcphdr {
    uint16_t source;
    uint16_t dest;
    uint32_t seq;
    uint32_t ack_seq;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint16_t res1:4;
    uint16_t doff:4;
    uint16_t fin:1;
    uint16_t syn:1;
    uint16_t rst:1;
    uint16_t psh:1;
    uint16_t ack:1;
    uint16_t urg:1;
    uint16_t ece:1;
    uint16_t cwr:1;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint16_t doff:4;
    uint16_t res1:4;
    uint16_t cwr:1;
    uint16_t ece:1;
    uint16_t urg:1;
    uint16_t ack:1;
    uint16_t psh:1;
    uint16_t rst:1;
    uint16_t syn:1;
    uint16_t fin:1;
#else
#error "Please fix <bytesex.h>"
#endif
    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
} __attribute__((packed));

typedef enum {
    KPF_TCP_CLOSED = 0,
    KPF_TCP_SYN_SENT,
    KPF_TCP_SYN_RECEIVED,
    KPF_TCP_ESTABLISHED,
    KPF_TCP_FIN_WAIT,
    KPF_TCP_CLOSE_WAIT,
    KPF_TCP_TIME_WAIT
} kpf_tcp_state_t;

typedef struct {
    uint32_t seed;
    uint32_t mutation_rate;
    bool corrupt_checksums;
    bool randomize_seq;
    size_t packet_count;
    char interface_name[IFNAMSIZ];
    bool dry_run;
} kpf_config_t;

typedef struct {
    uint8_t data[KPF_MAX_PACKET_SIZE];
    size_t length;
    uint32_t packet_index;
    kpf_tcp_state_t expected_state;
    kpf_tcp_state_t observed_state;
    bool has_crashed;
} kpf_packet_context_t;

// Core Engine API
void kpf_init_config(kpf_config_t *config);
uint32_t kpf_random_next(uint32_t *seed);
void kpf_craft_base_packet(kpf_packet_context_t *ctx, uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port);
void kpf_mutate_packet(kpf_packet_context_t *ctx, const kpf_config_t *config);
uint16_t kpf_calculate_ip_checksum(const struct kpf_iphdr *ip);
uint16_t kpf_calculate_tcp_checksum(const struct kpf_iphdr *ip, const struct kpf_tcphdr *tcp, const uint8_t *payload, size_t payload_len);
int kpf_inject_packet(int sockfd, const kpf_packet_context_t *ctx, const char *iface);
kpf_tcp_state_t kpf_track_tcp_state(kpf_tcp_state_t current, const struct kpf_tcphdr *tcp);
int kpf_write_pcap_record(const char *filename, const kpf_packet_context_t *ctx);

#endif
