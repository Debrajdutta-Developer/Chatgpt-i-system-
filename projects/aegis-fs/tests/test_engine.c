#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/engine.h"

int main(void) {
    printf("Running AegisFS Unit Tests...\n");

    AegisContext ctx;
    int init_res = aegis_init_context(&ctx, "./test_sandbox_unit");
    /* Assert 1: Context initialization succeeds */
    assert(init_res == 0);

    /* Assert 2: Context is marked initialized */
    assert(ctx.initialized == true);

    const char *test_str = "Hello AegisFS Content-Addressable Storage Engine World!";
    uint8_t hash1[AEGIS_HASH_SIZE];
    uint8_t hash2[AEGIS_HASH_SIZE];

    aegis_hash_bytes((const uint8_t *)test_str, strlen(test_str), hash1);
    aegis_hash_bytes((const uint8_t *)test_str, strlen(test_str), hash2);

    /* Assert 3: Hashing is deterministic */
    assert(memcmp(hash1, hash2, AEGIS_HASH_SIZE) == 0);

    /* Assert 4: Storing block succeeds */
    int store_res = aegis_store_block(&ctx, (const uint8_t *)test_str, strlen(test_str) + 1, hash1);
    assert(store_res == 0);

    uint8_t read_buf[256];
    size_t read_size = sizeof(read_buf);
    int read_res = aegis_read_block(&ctx, hash1, read_buf, &read_size);

    /* Assert 5: Reading stored block succeeds */
    assert(read_res == 0);

    /* Assert 6: Read payload matches original byte-for-byte */
    assert(strcmp((char *)read_buf, test_str) == 0);

    /* Assert 7: Chunking returns valid chunk count */
    size_t chunk_sizes[16];
    size_t chunks = aegis_rabin_chunk((const uint8_t *)test_str, strlen(test_str), chunk_sizes, 16);
    assert(chunks > 0);

    /* Assert 8: Compression roundtrip succeeds */
    uint8_t compressed[512];
    uint8_t decompressed[512];
    size_t comp_sz = aegis_compress((const uint8_t *)test_str, strlen(test_str), compressed, sizeof(compressed));
    assert(comp_sz > 0);

    size_t decomp_sz = aegis_decompress(compressed, comp_sz, decompressed, sizeof(decompressed));
    assert(decomp_sz == strlen(test_str));

    /* Assert 10: Decompressed data matches original */
    assert(memcmp(test_str, decompressed, decomp_sz) == 0);

    /* Assert 11: File write integration test */
    int fwrite_res = aegis_write_file(&ctx, "testfile.dat", (const uint8_t *)test_str, strlen(test_str) + 1);
    assert(fwrite_res == 0);

    /* Assert 12: File read integration test */
    uint8_t file_read_buf[256];
    size_t file_read_sz = sizeof(file_read_buf);
    int fread_res = aegis_read_file(&ctx, "testfile.dat", file_read_buf, &file_read_sz);
    assert(fread_res == 0);

    /* Assert 13: File content matches */
    assert(strcmp((char *)file_read_buf, test_str) == 0);

    /* Assert 14: Integrity check passes */
    int integrity = aegis_verify_integrity(&ctx);
    assert(integrity == 0);

    /* Assert 15: Destroy context succeeds */
    int destroy_res = aegis_destroy_context(&ctx);
    assert(destroy_res == 0);

    printf("All unit tests passed successfully!\n");
    return 0;
}
