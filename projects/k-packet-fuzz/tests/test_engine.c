#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine.h"

int main(void) {
    printf("Running engine unit tests...\n");

    // Assertion 1: Configuration initialization
    kpf_config_t config;
    kpf_init_config(&config);
    assert(config.seed == 42);

    // Assertion 2: Random number generator determinism
    uint32_t seed1 = 123;
    uint32_t r1 = kpf_random_next(&seed1);
    uint32_t seed2 = 123;
    uint32_t r2 = kpf_random_next(&seed2);
    assert(r1 == r2);

    // Assertion 3: Base packet crafting length
    kpf_packet_context_t ctx;
    kpf_craft_base_packet(&ctx, 0x0A000001, 0x0A000002, 4040, 8080);
    assert(ctx.length > sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr) + sizeof(struct kpf_tcphdr));

    // Assertion 4: IP Checksum validation
    const struct kpf_iphdr *ip = (const struct kpf_iphdr *)(ctx.data + sizeof(struct kpf_ethhdr));
    uint16_t ip_csum = kpf_calculate_ip_checksum(ip);
    assert(ip_csum != 0);

    // Assertion 5: TCP Checksum validation
    const struct kpf_tcphdr *tcp = (const struct kpf_tcphdr *)(ctx.data + sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr));
    size_t payload_len = ctx.length - sizeof(struct kpf_ethhdr) - sizeof(struct kpf_iphdr) - sizeof(struct kpf_tcphdr);
    uint16_t tcp_csum = kpf_calculate_tcp_checksum(ip, tcp, ctx.data + sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr) + sizeof(struct kpf_tcphdr), payload_len);
    assert(tcp_csum != 0);

    // Assertion 6: TCP State Tracker initial transition (SYN)
    kpf_tcp_state_t state = kpf_track_tcp_state(KPF_TCP_CLOSED, tcp);
    assert(state == KPF_TCP_SYN_SENT);

    // Assertion 7: Mutation alters packet content or metadata
    uint8_t original_data[KPF_MAX_PACKET_SIZE];
    memcpy(original_data, ctx.data, ctx.length);
    kpf_config_t mut_config;
    kpf_init_config(&mut_config);
    mut_config.mutation_rate = 100;
    kpf_mutate_packet(&ctx, &mut_config);
    assert(memcmp(original_data, ctx.data, ctx.length) != 0);

    // Assertion 8: Packet index increments correctly
    assert(ctx.packet_index == 1);

    // Assertion 9: Injection simulation returns positive packet length
    int inj_res = kpf_inject_packet(-1, &ctx, "lo");
    assert(inj_res == (int)ctx.length);

    // Assertion 10: Null pointer safety checks
    uint16_t null_ip_csum = kpf_calculate_ip_checksum(NULL);
    assert(null_ip_csum == 0);

    printf("All engine unit tests passed successfully (10 assertions verified).\n");
    return 0;
}
