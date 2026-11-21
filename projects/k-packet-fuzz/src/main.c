#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --seed <num>          Set pseudo-random seed\n");
    printf("  --count <num>         Number of fuzz iterations\n");
    printf("  --rate <num>          Mutation rate percentage (0-100)\n");
    printf("  --iface <name>        Network interface (default: lo)\n");
    printf("  --dry-run             Simulate fuzzing without socket injection\n");
    printf("  --output <file>       Write generated stream to pcap file\n");
    printf("  --help                Display this help message\n");
}

int main(int argc, char **argv) {
    kpf_config_t config;
    kpf_init_config(&config);
    
    char output_file[256] = "";
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            config.seed = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            config.packet_count = (size_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            config.mutation_rate = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--iface") == 0 && i + 1 < argc) {
            snprintf(config.interface_name, sizeof(config.interface_name), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            config.dry_run = true;
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            snprintf(output_file, sizeof(output_file), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    printf("Starting Kernel-Level Packet Fuzzer (k-packet-fuzz)\n");
    printf("Configuration: seed=%u, count=%zu, rate=%u%%, iface=%s, dry_run=%s\n",
           config.seed, config.packet_count, config.mutation_rate,
           config.interface_name, config.dry_run ? "true" : "false");
           
    kpf_packet_context_t ctx;
    kpf_craft_base_packet(&ctx, 0x7F000001, 0x7F000001, 12345, 80);
    
    size_t injected_success = 0;
    for (size_t i = 0; i < config.packet_count; ++i) {
        kpf_mutate_packet(&ctx, &config);
        
        const struct kpf_tcphdr *tcp = (const struct kpf_tcphdr *)(ctx.data + sizeof(struct kpf_ethhdr) + sizeof(struct kpf_iphdr));
        ctx.observed_state = kpf_track_tcp_state(ctx.observed_state, tcp);
        
        int res = kpf_inject_packet(-1, &ctx, config.interface_name);
        if (res > 0) {
            injected_success++;
        }
        
        if (output_file[0] != '\0') {
            kpf_write_pcap_record(output_file, &ctx);
        }
    }
    
    printf("Fuzzing completed successfully. Processed %zu packets, injected/simulated %zu packets.\n",
           config.packet_count, injected_success);
    return 0;
}
