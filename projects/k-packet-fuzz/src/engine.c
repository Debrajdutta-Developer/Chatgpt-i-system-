#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

void kpf_init_config(kpf_config_t *config) {
    if (!config) return;
    config->seed = 42;
    config->mutation_rate = 10; // 10% mutation chance per field
    config->corrupt_checksums = true;
    config->randomize_seq = true;
    config->packet_count = 100;
    snprintf(config->interface_name, sizeof(config->interface_name), "%s", KPF_DEFAULT_IFACE);
    config->dry_run = false;
}

uint32_t kpf_random_next(uint32_t *seed) {
    if (!seed) return 0;
    *seed = (*seed * 1103515245U) + 12345U;
    return *seed;
}

uint16_t kpf_calculate_ip_checksum(const struct kpf_iphdr *ip) {
    if (!ip) return 0;
    const uint16_t *ptr = (const uint16_t *)ip;
    uint32_t sum = 0;
    size_t len = (ip->ihl) * 4;
    
    // Temporarily clear check field for calculation
    struct kpf_iphdr temp_ip = *ip;
    temp_ip.check = 0;
    ptr = (const uint16_t *)&temp_ip;

    for (size_t i = 0; i < len / 2; ++i) {
        sum += ntohs(ptr[i]);
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return htons((uint16_t)(~sum));
}

uint16_t kpf_calculate_tcp_checksum(const struct kpf_iphdr *ip, const struct kpf_tcphdr *tcp, const uint8_t *payload, size_t payload_len) {
    if (!ip || !tcp) return 0;
    uint32_t sum = 0;
    
    // Pseudo header sum
    sum += (ip->saddr >> 16) & 0xFFFF;
    sum += ip->saddr & 0xFFFF;
    sum += (ip->daddr >> 16) & 0xFFFF;
    sum += ip->daddr & 0xFFFF;
    sum += htons(IPPROTO_TCP);
    
    size_t tcp_total_len = sizeof(struct kpf_tcphdr) + payload_len;
    sum += htons((uint16_t)tcp_total_len);
    
    // TCP Header sum (with check = 0)
    struct kpf_tcphdr temp_tcp = *tcp;
    temp_tcp.check = 0;
    const uint16_t *tcp_ptr = (const uint16_t *)&temp_tcp;
    for (size_t i = 0; i < sizeof(struct kpf_tcphdr) / 2; ++i) {
        sum += ntohs(tcp_ptr[i]);
    }
    
    // Payload sum
    const uint16_t *payload_ptr = (const uint16_t *)payload;
    for (size_t i = 0; i < payload_len / 2; ++i) {
        sum += ntohs(payload_ptr[i]);
    }
    if (payload_len % 2 != 0) {
        sum += (uint32_t)(((const uint8_t *)payload)[payload_len - 1]) << 8;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return htons((uint16_t)(~sum));
}

void kpf_craft_base_packet(kpf_packet_context_t *ctx, uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port) {
    if (!ctx) return;
    memset(ctx->data, 0, KPF_MAX_PACKET_SIZE);
    
    struct kpf_ethhdr *eth = (struct kpf_ethhdr *)ctx->data;
    for (int i = 0; i < KPF_ETH_ALEN; ++i) {
        eth->h_dest[i] = (uint8_t)(i + 1);
        eth->h_source[i] = (uint8_t)(i + 0xA0);
    }
    eth->h_proto = htons(0x0800); // IPv4
    
    struct kpf_iphdr *ip = (struct kpf_iphdr *)(ctx->data + sizeof(struct kpf_ethhdr));
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = htons(sizeof(struct kpf_iphdr) + sizeof(struct kpf_tcphdr) + 16);
    ip->id = htons(54321);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = IPPROTO_TCP;
    ip->saddr = htonl(src_ip);
    ip->daddr = htonl(dst_ip);
    ip->check = kpf_calculate_ip_checksum(ip);
    
    struct kpf_tcphdr *tcp = (struct kpf_tcphdr *)(ctx->data + sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr));
    tcp->source = htons(src_port);
    tcp->dest = htons(dst_port);
    tcp->seq = htonl(1000);
    tcp->ack_seq = htonl(0);
    tcp->doff = 5;
    tcp->syn = 1;
    tcp->ack = 0;
    tcp->window = htons(8192);
    tcp->urg_ptr = 0;
    
    uint8_t *payload = ctx->data + sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr) + sizeof(struct kpf_tcphdr);
    const char *msg = "KPF_TEST_PAYLOAD";
    memcpy(payload, msg, 16);
    
    tcp->check = kpf_calculate_tcp_checksum(ip, tcp, payload, 16);
    
    ctx->length = sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr) + sizeof(struct kpf_tcphdr) + 16;
    ctx->packet_index = 0;
    ctx->expected_state = KPF_TCP_SYN_SENT;
    ctx->observed_state = KPF_TCP_CLOSED;
    ctx->has_crashed = false;
}

void kpf_mutate_packet(kpf_packet_context_t *ctx, const kpf_config_t *config) {
    if (!ctx || !config) return;
    uint32_t seed = config->seed + ctx->packet_index;
    
    struct kpf_iphdr *ip = (struct kpf_iphdr *)(ctx->data + sizeof(struct kpf_ethhdr));
    struct kpf_tcphdr *tcp = (struct kpf_tcphdr *)(ctx->data + sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr));
    size_t eth_ip_len = sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr);
    size_t payload_len = ctx->length > eth_ip_len + sizeof(struct kpf_tcphdr) ? ctx->length - eth_ip_len - sizeof(struct kpf_tcphdr) : 0;
    uint8_t *payload = ctx->data + eth_ip_len + sizeof(struct kpf_tcphdr);

    if ((kpf_random_next(&seed) % 100) < config->mutation_rate) {
        ip->id ^= (uint16_t)(kpf_random_next(&seed) & 0xFFFF);
    }
    if ((kpf_random_next(&seed) % 100) < config->mutation_rate) {
        tcp->window ^= (uint16_t)(kpf_random_next(&seed) & 0xFFFF);
    }
    if (config->randomize_seq && ((kpf_random_next(&seed) % 100) < config->mutation_rate)) {
        tcp->seq ^= kpf_random_next(&seed);
    }
    if (payload_len > 0 && ((kpf_random_next(&seed) % 100) < config->mutation_rate)) {
        size_t idx = kpf_random_next(&seed) % payload_len;
        payload[idx] ^= (uint8_t)(kpf_random_next(&seed) & 0xFF);
    }

    if (config->corrupt_checksums && ((kpf_random_next(&seed) % 100) < (config->mutation_rate / 2))) {
        ip->check += (uint16_t)(kpf_random_next(&seed) & 0xF);
        tcp->check += (uint16_t)(kpf_random_next(&seed) & 0xF);
    } else {
        ip->check = kpf_calculate_ip_checksum(ip);
        tcp->check = 0;
        tcp->check = kpf_calculate_tcp_checksum(ip, tcp, payload, payload_len);
    }
    
    ctx->packet_index++;
}

int kpf_inject_packet(int sockfd, const kpf_packet_context_t *ctx, const char *iface) {
    (void)sockfd;
    (void)iface;
    if (!ctx) return -1;
    // In dry-run or user-space simulation mode, return packet length as successful injection indicator
    if (ctx->length == 0) return -1;
    return (int)ctx->length;
}

kpf_tcp_state_t kpf_track_tcp_state(kpf_tcp_state_t current, const struct kpf_tcphdr *tcp) {
    if (!tcp) return current;
    switch (current) {
        case KPF_TCP_CLOSED:
            if (tcp->syn && !tcp->ack) return KPF_TCP_SYN_SENT;
            break;
        case KPF_TCP_SYN_SENT:
            if (tcp->syn && tcp->ack) return KPF_TCP_ESTABLISHED;
            if (tcp->rst) return KPF_TCP_CLOSED;
            break;
        case KPF_TCP_ESTABLISHED:
            if (tcp->fin) return KPF_TCP_FIN_WAIT;
            if (tcp->rst) return KPF_TCP_CLOSED;
            break;
        case KPF_TCP_FIN_WAIT:
            if (tcp->ack) return KPF_TCP_CLOSED;
            break;
        default:
            break;
    }
    return current;
}

int kpf_write_pcap_record(const char *filename, const kpf_packet_context_t *ctx) {
    if (!filename || !ctx) return -1;
    FILE *f = fopen(filename, "ab");
    if (!f) return -1;
    
    // Write pseudo pcap record header (ts_sec, ts_usec, incl_len, orig_len)
    uint32_t header[4];
    header[0] = 1700000000;
    header[1] = 0;
    header[2] = (uint32_t)ctx->length;
    header[3] = (uint32_t)ctx->length;
    
    fwrite(header, sizeof(uint32_t), 4, f);
    fwrite(ctx->data, 1, ctx->length, f);
    fclose(f);
    return 0;
}
