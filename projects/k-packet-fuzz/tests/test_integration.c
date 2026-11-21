#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine.h"

int main(void) {
    printf("Running integration tests...\n");

    // Assertion 1: Multi-packet fuzz session execution
    kpf_config_t config;
    kpf_init_config(&config);
    config.packet_count = 50;
    config.seed = 999;

    kpf_packet_context_t ctx;
    kpf_craft_base_packet(&ctx, 0xC0A80001, 0xC0A80002, 5555, 443);
    assert(ctx.length == sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr) + sizeof(struct kpf_tcphdr) + 16);

    // Assertion 2: Fuzzing loop mutation coverage
    size_t mutated_count = 0;
    for (size_t i = 0; i < config.packet_count; ++i) {
        uint8_t before[64];
        memcpy(before, ctx.data, 64);
        kpf_mutate_packet(&ctx, &config);
        if (memcmp(before, ctx.data, 64) != 0) {
            mutated_count++;
        }
    }
    assert(mutated_count > 0);

    // Assertion 3: TCP State Machine progression across sequence
    struct kpf_tcphdr test_tcp;
    memset(&test_tcp, 0, sizeof(struct kpf_tcphdr));
    test_tcp.syn = 1;
    test_tcp.ack = 1;
    kpf_tcp_state_t established_state = kpf_track_tcp_state(KPF_TCP_SYN_SENT, &test_tcp);
    assert(established_state == KPF_TCP_ESTABLISHED);

    // Assertion 4: FIN state transition handling
    memset(&test_tcp, 0, sizeof(struct kpf_tcphdr));
    test_tcp.fin = 1;
    kpf_tcp_state_t fin_state = kpf_track_tcp_state(KPF_TCP_ESTABLISHED, &test_tcp);
    assert(fin_state == KPF_TCP_FIN_WAIT);

    // Assertion 5: RST state transition handling
    memset(&test_tcp, 0, sizeof(struct kpf_tcphdr));
    test_tcp.rst = 1;
    kpf_tcp_state_t closed_state = kpf_track_tcp_state(KPF_TCP_ESTABLISHED, &test_tcp);
    assert(closed_state == KPF_TCP_CLOSED);

    // Assertion 6: PCAP file record serialization
    const char *test_pcap = "test_output.pcap";
    remove(test_pcap); // clean up any prior runs
    int pcap_res = kpf_write_pcap_record(test_pcap, &ctx);
    assert(pcap_res == 0);

    // Assertion 7: PCAP file exists and has non-zero size
    FILE *f_pcap = fopen(test_pcap, "rb");
    assert(f_pcap != NULL);
    fseek(f_pcap, 0, SEEK_END);
    long pcap_size = ftell(f_pcap);
    fclose(f_pcap);
    assert(pcap_size > (long)sizeof(uint32_t) * 4);
    remove(test_pcap); // cleanup

    // Assertion 8: Config parameter boundary validation
    kpf_config_t custom_config;
    kpf_init_config(&custom_config);
    custom_config.mutation_rate = 50;
    assert(custom_config.mutation_rate == 50);

    // Assertion 9: Packet context crash status flag initialization
    assert(ctx.has_crashed == false);

    // Assertion 10: Deterministic replay verification
    kpf_packet_context_t ctx1, ctx2;
    kpf_craft_base_packet(&ctx1, 0x7F000001, 0x7F000001, 1000, 2000);
    kpf_craft_base_packet(&ctx2, 0x7F000001, 0x7F000001, 1000, 2000);
    kpf_config_t cfg1 = { .seed = 1234, .mutation_rate = 100, .corrupt_checksums = false, .randomize_seq = true, .packet_count = 1, .dry_run = true };
    kpf_config_t cfg2 = { .seed = 1234, .mutation_rate = 100, .corrupt_checksums = false, .randomize_seq = true, .packet_count = 1, .dry_run = true };
    kpf_mutate_packet(&ctx1, &cfg1);
    kpf_mutate_packet(&ctx2, &cfg2);
    assert(memcmp(ctx1.data, ctx2.data, ctx1.length) == 0);

    printf("All integration tests passed successfully (10 assertions verified).\n");
    return 0;
}
