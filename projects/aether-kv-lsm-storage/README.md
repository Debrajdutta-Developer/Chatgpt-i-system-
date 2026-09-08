# AetherKV - Log-Structured Merge-Tree (LSM) Storage Engine

## Architecture

AetherKV is a zero-dependency, high-performance, embeddable Log-Structured Merge-Tree (LSM) storage engine written in Java 21. It is designed for resource-constrained environments requiring structured SQL-like querying capabilities, low-latency writes, and robust crash-recovery guarantees.

```
+-------------------------------------------------------------------------+
|                           AetherKV Engine                               |
|                                                                         |
|  +------------------+      +------------------+      +---------------+  |
|  |   SQL Parser     | ---> |   Active MemTable| ---> |  Active WAL   |  |
|  +------------------+      | (SkipListMap)    |      |  (wal_*.log)  |  |
|                            +------------------+      +---------------+  |
|                                     |                                   |
|                                     v (Flush)                           |
|                            +------------------+                         |
|                            |   SSTable Files  |                         |
|                            |  (sstable_*.db)  |                         |
|                            +------------------+                         |
|                                     |                                   |
|                                     v (Compaction)                      |
|                            +------------------+                         |
|                            | Compacted SSTable|                         |
|                            +------------------+                         |
+-------------------------------------------------------------------------+
```

### Core Components
1. **SQL Parser & Executor**: Parses a subset of SQL (`SELECT`, `INSERT`, `DELETE`, `WHERE`) and executes them directly on byte-serialized row schemas.
2. **MemTable**: An in-memory, concurrent skip-list map (`ConcurrentSkipListMap`) that buffers incoming writes. It is bounded by a dynamic memory budget monitor.
3. **Write-Ahead Log (WAL)**: A sequential binary log that records all mutations before they are applied to the MemTable, ensuring transactional durability.
4. **SSTables (Sorted String Tables)**: Immutable on-disk files containing sorted key-value pairs, complete with sparse block indexes and serialized Bloom filters to minimize disk I/O.
5. **Compaction Engine**: A background worker utilizing Java 21 Virtual Threads to merge overlapping SSTables, reclaiming space and maintaining read performance.
6. **MVCC (Multi-Version Concurrency Control)**: Implements snapshot isolation by tagging every mutation with a monotonically increasing version number.

---

## Algorithms and Data Structures

- **Concurrent Skip-List Map**: Used for the active MemTable to allow lock-free concurrent reads and writes while maintaining sorted order.
- **Sparse Block Index**: SSTables are indexed sparsely (e.g., every 16 keys). This allows binary searching the index in memory to locate the exact data block on disk, keeping memory overhead minimal.
- **Bloom Filter**: A custom bitset-based Bloom filter is built for each SSTable. It uses double-hashing (`hashCode` and a secondary hash) to generate `k` hash functions, preventing redundant disk reads for non-existent keys.
- **Size-Tiered Compaction**: Merges multiple SSTables into a single consolidated SSTable using an in-memory merge-sort, ensuring zero duplicate keys and removing obsolete tombstones.
- **Snapshot Isolation**: Readers capture the current global version at the start of their transaction. They only see keys with a version less than or equal to their read version, preventing dirty or non-repeatable reads.

---

## Invariants

- **Write-Ahead Durability**: No write is acknowledged to the client until it has been successfully appended to the active WAL file and synced to disk via `FileDescriptor.sync()`.
- **Monotonic Versioning**: Every mutation is assigned a strictly increasing version number. On startup, the engine scans all SSTables and WALs to restore the global version counter to a value higher than any previously written version.
- **SSTable Immutability**: Once written, SSTable files are never modified. Compaction creates new SSTables and atomically swaps them before deleting the old ones.
- **Tombstone Semantics**: Deletions are written as tombstones with a special flag. During reads, tombstones mask older versions of the key. During compaction, tombstones are purged if no older versions exist in any active SSTable.

---

## Build and Run

### Prerequisites
- Java 21 or higher
- Maven (optional, or compile directly using `javac`)

### Compilation
To compile the project using standard Java tools:
```bash
mkdir -p bin
javac -d bin src/main/java/factory/*.java
```

### Running the Server
To start the embedded TCP server on port `8080`:
```bash
java -cp bin factory.Main --dir ./data --port 8080
```

### Running Queries via CLI
You can also execute a single query directly from the command line:
```bash
java -cp bin factory.Main --dir ./data --query "INSERT INTO users VALUES ('u1', 'Alice', 30, true)"
java -cp bin factory.Main --dir ./data --query "SELECT * FROM users WHERE age > 25"
```

---

## Testing

We provide comprehensive unit and integration tests that verify crash recovery, compaction, Bloom filters, and SQL parsing.

To compile and run the tests:
```bash
javac -d bin src/main/java/factory/*.java src/test/java/factory/*.java
java -cp bin factory.CoreTest
java -cp bin factory.IntegrationTest
```

---

## Security and Privacy

- **Zero External Dependencies**: The project relies entirely on the standard Java library, eliminating supply-chain vulnerabilities.
- **Local Storage**: All data is stored locally in the designated directory, ensuring complete data sovereignty.
- **Input Validation**: The SQL parser strictly validates inputs using regular expressions to prevent SQL injection or malformed data corruption.

---

## Performance Characteristics

- **Write Path**: $O(1)$ sequential disk writes (WAL) followed by $O(\log N)$ in-memory skip-list insertion. Extremely high write throughput.
- **Read Path**: $O(1)$ Bloom filter check, followed by $O(\log (N/B))$ binary search on the sparse index, and finally $O(B)$ sequential scan of the target block (where $B$ is the sparse interval block size).
- **Compaction**: Offloaded to Java 21 Virtual Threads, ensuring zero latency spikes on the main read/write paths.

---

## Limitations

- **Schema Rigidity**: The current SQL engine supports a fixed schema (`id` VARCHAR, `name` VARCHAR, `age` INT, `active` BOOLEAN) for simplicity and performance.
- **In-Memory Compaction**: The compaction engine reads SSTable entries into memory to perform the merge-sort. For extremely large datasets, a multi-way streaming merge-sort would be required.
- **No Joins**: The SQL engine is designed for single-table key-value operations and does not support relational joins or complex aggregations.
