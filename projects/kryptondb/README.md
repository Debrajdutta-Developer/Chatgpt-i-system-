# KryptonDB

KryptonDB is a zero-dependency embedded document database engine written in Java 21, designed specifically for embedded systems engineers and local-first application developers who require robust data management without the overhead of heavy external database servers or fragile flat files.

## Architecture

KryptonDB utilizes a layered, modular storage architecture built entirely on standard Java 21 APIs. At the foundation is a Slotted-Page Storage Allocator that maps physical files into fixed-size 4KB pages containing slotted headers for variable-length document packing. Above the storage layer sits a Binary Write-Ahead Log (WAL) with CRC32 checksums, ensuring transactional durability and crash safety. Indexing is powered by a custom B+Tree on-disk index manager with fixed-size node splitting and merging. Concurrency and transaction isolation are governed by a Multi-Version Concurrency Control (MVCC) snapshot isolation engine coordinated by Java 21 Virtual Threads, backed by a background checkpointing daemon that flushes dirty cache pages and prunes old WAL segments. Query evaluation is driven by a custom JSON-like query parser supporting JSON-path filtering, projections, and condition matching.

## Algorithms and Data Structures

- **Slotted-Page Allocator**: Manages 4KB disk blocks with a header tracking free space offsets and record counts, allowing variable-length byte arrays (documents) to be inserted, updated, and deleted without internal file fragmentation.
- **B+Tree Index**: Self-balancing tree structure maintaining keys and slotted-page identifiers. Supports node splitting when capacity is exceeded and node merging/redistribution underflow handling.
- **MVCC Snapshot Isolation**: Maintains transaction sequence numbers (`txId`). Readers access consistent views of document versions based on transaction start timestamps, while writers acquire exclusive locks on document keys to prevent write skew.
- **Binary WAL with CRC32**: Records transactional operations (`BEGIN`, `INSERT`, `UPDATE`, `DELETE`, `COMMIT`, `ABORT`) prefixed with length, transaction ID, operation type, payload, and a 4-byte CRC32 checksum to detect bit rot or truncated writes.
- **Query Parser**: A recursive descent expression evaluator that parses filter expressions (e.g., `field = value`, `field > value`) and projections over flattened map structures.

## Invariants

1. **Durability Invariant**: No transaction is reported as committed until its commit record and all associated data updates are flushed to the WAL file with valid CRC32 checksums.
2. **MVCC Consistency Invariant**: A transaction never reads uncommitted changes from concurrent transactions; reader snapshots are strictly bounded by transaction start timestamps.
3. **B+Tree Integrity Invariant**: All leaf nodes remain linked in sorted order, and internal node keys correctly bound the sub-tree key ranges.
4. **Storage Allocator Invariant**: Slotted page free space pointers never exceed the 4KB boundary, preventing buffer overflows and corrupted page writes.
5. **Crash Recovery Invariant**: Replaying the WAL on startup restores the database to the exact state of the last committed transaction, discarding uncommitted or partial transactions.

## Build and Run

KryptonDB requires Java 21. No external build tools or libraries are required.

Compile the codebase:
```bash
javac src/main/java/factory/Core.java src/main/java/factory/Main.java src/test/java/factory/CoreTest.java src/test/java/factory/IntegrationTest.java
```

Run the executable CLI / demonstration runner:
```bash
java -cp src/main/java:src/test/java factory.Main
```

Run unit tests:
```bash
java -cp src/main/java:src/test/java factory.CoreTest
```

Run integration tests:
```bash
java -cp src/main/java:src/test/java factory.IntegrationTest
```

## Testing

KryptonDB includes comprehensive unit and integration test suites (`CoreTest` and `IntegrationTest`). Each test suite executes at least 10 independent assertion invocations checking malformed JSON documents, concurrent transaction rollbacks, B+Tree split/merge behavior, query filtering, and simulated disk corruption recovery. All tests execute against the real engine without mocks or stubs.

## Security and Privacy

KryptonDB operates entirely locally within the application's process space and file system. It does not open network sockets, perform telemetry, or access external resources. Data encryption at rest can be layered above the raw storage files by wrapping standard `RandomAccessFile` or channel abstractions.

## Performance Characteristics

- **Zero Dependency**: Extremely small memory footprint; suitable for resource-constrained embedded environments.
- **Virtual Threads**: Leverages Java 21 virtual threads for highly concurrent transaction coordination without heavy OS thread switching overheads.
- **Buffered I/O**: Efficient page caching minimizes disk reads during frequent point lookups and range scans.
- **Asynchronous Checkpointing**: Background daemon periodically flushes dirty pages, reducing restart recovery times.

## Limitations

- Single-node embedded architecture (no distributed consensus or replication).
- Fixed 4KB page size limits maximum individual document size to slightly under 4KB unless split across multiple slotted pages.
- Query engine supports basic equality, comparison, and boolean combination operators rather than full SQL syntax.
