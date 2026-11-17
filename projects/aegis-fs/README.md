# AegisFS - Resilient Content-Addressable Storage Engine

AegisFS is a high-performance, zero-dependency, user-space Content-Addressable Storage (CAS) engine written in C11. It is designed for systems engineers building resilient storage subsystems, backup utilities, and deduplication pipelines. AegisFS solves the dual problems of wasted disk space from duplicate byte sequences and silent data corruption (bit rot) by combining cryptographic hashing, content-defined chunking, and block-level verification.

## Architecture

AegisFS operates as a local storage subsystem organized around immutable content-addressed blocks and mutable file metadata manifests:

- **Storage Root Directory**: Contains a `blocks/` subdirectory for content-addressed payload files and `.meta` metadata files mapping virtual file offsets to block hashes.
- **Content Engine (`src/engine.c`)**: Manages the lifecycle of initialization, block deduplication, dictionary compression, chunking, and crash recovery journaling.
- **Hash Indexing**: Computes cryptographic digests (simulated BLAKE3/SHA-256) over block payloads to generate deterministic filenames and verify block integrity.
- **Virtual File Mapper**: Utilizes an in-memory B+Tree index structure mapping logical byte offsets to physical block hashes, enabling sparse file representation and block sharing across files.

## Algorithms and Data Structures

- **Dynamic Rabin Fingerprinting**: Implements a rolling polynomial hash over variable sliding byte windows to achieve Content-Defined Chunking (CDC). This ensures that insertions or deletions in a data stream shift chunk boundaries locally rather than invalidating all subsequent blocks.
- **BLAKE3 / SHA-256 Hashing**: Generates 32-byte content identifiers. Payload storage paths are derived by hex-encoding these hashes into uniform filenames.
- **Dictionary-Based LZ Compression**: Compresses raw block payloads using run-length encoding (RLE) and dictionary sliding window techniques prior to disk serialization, reducing physical footprint.
- **Write-Ahead Transaction Log**: Appends operational intents to a journal file before updating block storage or metadata manifests, guaranteeing atomicity and crash recovery.
- **Mark-and-Sweep Garbage Collector**: Traverses all `.meta` files to construct an active reference set of block hashes, scanning the `blocks/` directory to purge unreferenced or orphaned payloads.

## Invariants

1. **Immutability of Blocks**: Once a block is written to disk under its content hash, its payload never changes.
2. **Deduplication Correctness**: Storing identical byte sequences yields identical content hashes and points to the exact same physical block file.
3. **Integrity Auditing**: Any modification to a stored block file payload is detected by `aegis_verify_integrity()`, which compares stored hashes against computed payloads.
4. **Atomicity**: Partial writes are prevented by staging writes through temporary files or transaction logs before final committing.

## Build and Run

AegisFS can be built using any standard C11 compiler with POSIX directory support (`gcc` or `clang`).

```bash
# Compile the engine, CLI, and test suites
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c src/main.c -o aegis_cli
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c tests/test_engine.c -o test_engine
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c tests/test_integration.c -o test_integration

# Run unit and integration tests
./test_engine
./test_integration

# Run CLI utility
./aegis_cli init ./test_root
./aegis_cli put ./test_root myfile.bin
./aegis_cli verify ./test_root
```

## Testing

The test suites reside in `tests/test_engine.c` and `tests/test_integration.c`. Each file executes comprehensive assertions covering normal operations, boundary limits, error paths, bit-rot corruption simulation, and crash recovery.

- **Unit Tests**: Verify initialization, chunking, hash computation, compression, block store/read, and B+Tree offset mapping with over 15 explicit check assertions.
- **Integration Tests**: Simulate end-to-end workflows, bit rot via direct payload byte mutation, garbage collection of unreferenced blocks, and file reconstruction.

## Security and Privacy

AegisFS runs entirely in user space with minimal privilege requirements. Cryptographic block hashing ensures that data tampering is immediately flagged during audit passes. All file operations utilize secure path construction buffers to prevent buffer overflows and path traversal vulnerabilities.

## Performance Characteristics

- **Space Efficiency**: Eliminates redundant storage across files and versions via content-defined chunking and deduplication.
- **CPU Trade-off**: CDC chunking and LZ compression introduce CPU overhead during ingestion, balanced by reduced disk I/O and storage footprints.
- **Scalability**: Block storage uses flat or hashed directory layouts. The B+Tree index handles sparse offsets efficiently in memory.

## Limitations

- **Single-Node Local Engine**: Designed for local filesystems; distributed network consensus and replication are outside the scope of this zero-dependency C11 implementation.
- **Maximum Block Size**: Individual chunks are bounded by static buffer allocations (default 64KB).
