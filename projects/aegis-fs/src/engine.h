#ifndef AEGIS_ENGINE_H
#define AEGIS_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AEGIS_MAX_PATH 1024
#define AEGIS_HASH_SIZE 32
#define AEGIS_BLOCK_MAX (64 * 1024)

typedef struct {
    char root_path[AEGIS_MAX_PATH];
    bool initialized;
} AegisContext;

typedef struct {
    uint64_t virtual_offset;
    uint32_t length;
    uint8_t hash[AEGIS_HASH_SIZE];
} AegisBlockRef;

typedef struct {
    uint64_t total_size;
    size_t block_count;
    AegisBlockRef *blocks;
} AegisMetadata;

/* Core Engine Lifecycle */
int aegis_init_context(AegisContext *ctx, const char *root_path);
int aegis_destroy_context(AegisContext *ctx);

/* Block Store & Retrieve */
int aegis_store_block(AegisContext *ctx, const uint8_t *data, size_t size, uint8_t *out_hash);
int aegis_read_block(AegisContext *ctx, const uint8_t *hash, uint8_t *out_data, size_t *inout_size);

/* Chunking & Compression */
size_t aegis_rabin_chunk(const uint8_t *data, size_t size, size_t *out_chunk_sizes, size_t max_chunks);
size_t aegis_compress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);
size_t aegis_decompress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);

/* File-level Operations */
int aegis_write_file(AegisContext *ctx, const char *filename, const uint8_t *data, size_t size);
int aegis_read_file(AegisContext *ctx, const char *filename, uint8_t *out_data, size_t *inout_size);

/* Maintenance & Auditing */
int aegis_verify_integrity(AegisContext *ctx);
int aegis_garbage_collect(AegisContext *ctx);
int aegis_recover_journal(AegisContext *ctx);

/* Hashing Utility */
void aegis_hash_bytes(const uint8_t *data, size_t size, uint8_t *out_hash);

#endif
