#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "engine.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: aegis_cli <init|put|get|verify> <root_path> [filename]\n");
        return 1;
    }

    const char *cmd = argv[1];
    const char *root = argv[2];

    AegisContext ctx;
    if (aegis_init_context(&ctx, root) != 0) {
        fprintf(stderr, "Failed to initialize AegisFS context at %s\n", root);
        return 1;
    }

    if (strcmp(cmd, "init") == 0) {
        printf("AegisFS initialized successfully at %s\n", root);
    } else if (strcmp(cmd, "put") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: put requires filename\n");
            aegis_destroy_context(&ctx);
            return 1;
        }
        const char *fname = argv[3];
        uint8_t data[1024];
        int r = snprintf((char *)data, sizeof(data), "Sample payload content for %s", fname);
        if (r < 0 || (size_t)r >= sizeof(data)) {
            aegis_destroy_context(&ctx);
            return 1;
        }
        if (aegis_write_file(&ctx, fname, data, strlen((char *)data) + 1) == 0) {
            printf("Successfully wrote and deduplicated file %s\n", fname);
        } else {
            fprintf(stderr, "Failed to write file %s\n", fname);
            aegis_destroy_context(&ctx);
            return 1;
        }
    } else if (strcmp(cmd, "get") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: get requires filename\n");
            aegis_destroy_context(&ctx);
            return 1;
        }
        const char *fname = argv[3];
        uint8_t buf[2048];
        size_t sz = sizeof(buf);
        if (aegis_read_file(&ctx, fname, buf, &sz) == 0) {
            printf("Read file %s (%zu bytes): %s\n", fname, sz, buf);
        } else {
            fprintf(stderr, "Failed to read file %s\n", fname);
            aegis_destroy_context(&ctx);
            return 1;
        }
    } else if (strcmp(cmd, "verify") == 0) {
        int res = aegis_verify_integrity(&ctx);
        if (res == 0) {
            printf("Integrity check passed with zero corrupted blocks.\n");
        } else {
            printf("Integrity check detected %d issues.\n", res);
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        aegis_destroy_context(&ctx);
        return 1;
    }

    aegis_destroy_context(&ctx);
    return 0;
}
