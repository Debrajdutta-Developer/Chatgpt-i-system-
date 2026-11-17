#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/engine.h"

int main(void) {
    printf("Running AegisFS Integration Tests...\n");

    AegisContext ctx;
    /* Assert 1: Sandbox initialization */
    assert(aegis_init_context(&ctx, "./test_sandbox_integration") == 0);

    const char *dataset_a = "Dataset Alpha: Redundant blocks for deduplication testing across multiple files.";
    const char *dataset_b = "Dataset Beta: Redundant blocks for deduplication testing across multiple files.";

    /* Assert 2: Write first dataset */
    assert(aegis_write_file(&ctx, "file_a.txt", (const uint8_t *)dataset_a, strlen(dataset_a) + 1) == 0);

    /* Assert 3: Write duplicate dataset to verify deduplication */
    assert(aegis_write_file(&ctx, "file_b.txt", (const uint8_t *)dataset_b, strlen(dataset_b) + 1) == 0);

    uint8_t out_a[256];
    uint8_t out_b[256];
    size_t sz_a = sizeof(out_a);
    size_t sz_b = sizeof(out_b);

    /* Assert 4: Read back dataset A */
    assert(aegis_read_file(&ctx, "file_a.txt", out_a, &sz_a) == 0);

    /* Assert 5: Read back dataset B */
    assert(aegis_read_file(&ctx, "file_b.txt", out_b, &sz_b) == 0);

    /* Assert 6: Dataset contents match expectations */
    assert(strcmp((char *)out_a, dataset_a) == 0);
    assert(strcmp((char *)out_b, dataset_b) == 0);

    /* Assert 7: Integrity verification passes before corruption */
    assert(aegis_verify_integrity(&ctx) == 0);

    /* Assert 8: Garbage collection runs successfully */
    assert(aegis_garbage_collect(&ctx) == 0);

    /* Assert 9: Journal recovery simulation succeeds */
    assert(aegis_recover_journal(&ctx) == 0);

    /* Assert 10: Context cleanup succeeds */
    assert(aegis_destroy_context(&ctx) == 0);

    printf("All integration tests passed successfully!\n");
    return 0;
}
