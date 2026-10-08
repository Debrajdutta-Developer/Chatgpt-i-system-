# ChronoGraph DB

ChronoGraph DB is a zero-dependency temporal graph database engine written in Java 21, designed specifically for database engineers and systems architects building auditing, lineage, and historical relationship-tracking systems. Standard graph databases represent only the current network state, forcing expensive full-history scans or complex manual node/edge duplication for temporal queries. ChronoGraph DB solves this by managing an on-disk append-only log of graph mutations, building temporal B+Tree indexes, and evaluating pathfinding queries (BFS, DFS, Dijkstra) integrated with point-in-time snapshot filters using Multi-Version Concurrency Control (MVCC).

## Architecture

The architecture of ChronoGraph DB is split into several clean storage, indexing, and execution layers:
- **Storage Layer**: An append-only log file (`chronograph.log`) that persists serialized vertex mutations, edge mutations, and commit markers using explicit binary formats with CRC32 checksums.
- **Indexing Layer**: In-memory temporal index structures mapping string identifiers and valid-time ranges to byte offsets within the append-only log. B+Tree indexing tracks key ranges for fast point lookups.
- **Execution Layer**: A multi-version query engine that accepts temporal snapshot filters (e.g., valid at microsecond $T$). It utilizes Java 21 virtual threads (`Executors.newVirtualThreadPerTaskExecutor()`) to execute concurrent pathfinding algorithms (BFS, DFS, Dijkstra) over historical subgraphs without lock contention.
- **Recovery & MVCC Layer**: Crash recovery scans the append-only log, re-verifies checksums, discards uncommitted partial tails, and reconstructs the concurrent snapshot index. Atomic state transitions are managed via standard synchronization and thread-safe data structures.

## Algorithms and Data Structures

- **Temporal Record Formats**: Vertices and edges carry immutable valid-time intervals `[validFrom, validTo)`. When a property changes or an edge is updated, the previous record is logically closed by setting its `validTo` timestamp, and a new record is appended.
- **Pathfinding Engines**: Breadth-First Search (BFS), Depth-First Search (DFS), and Dijkstra's shortest-path algorithm are implemented over temporal snapshot views. Edges and vertices that were inactive at the query snapshot time are completely invisible to the traversal traversal frontier.
- **Crash Recovery**: Sequential scan of the binary log file reads length-prefixed mutation records, validates CRC32 integrity, and builds the in-memory spatial-temporal index. If truncation or corruption is detected at the tail, the engine safely halts recovery at the last valid checkpoint.
- **Compaction & Garbage Collection**: Out-of-retention records whose `validTo` precedes the global retention watermark are coalesced to reclaim storage space.

## Invariants

1. **Immutability of Log Records**: Once written to disk, mutation records in the append-only log are never modified in place.
2. **Valid-Time Monotonicity**: For any logical entity, successive versions maintain non-overlapping, contiguous valid-time intervals `[t1, t2)` and `[t2, t3)`.
3. **Snapshot Isolation**: Queries executing at snapshot time $T$ observe exactly the state of the graph valid at $T$, isolated from concurrent uncommitted or future mutations.
4. **Log Integrity**: Every log record contains a valid CRC32 checksum over its payload; corrupted records trigger recovery aborts to prevent index pollution.

## Build and Run

Compile and test using standard JDK 21 tools:

```bash
javac src/main/java/factory/*.java src/test/java/factory/*.java
java factory.Main
java factory.CoreTest
java factory.IntegrationTest
```

## Testing

ChronoGraph DB includes comprehensive test suites (`CoreTest` and `IntegrationTest`) containing over 20 explicit assertion checks verifying:
- Point-in-time historical vertex and edge retrieval.
- Temporal pathfinding (BFS, DFS, Dijkstra) across snapshot intervals.
- Crash recovery and log corruption resilience with checksum verification.
- Concurrent multi-threaded reads under virtual thread execution.
- Mutation logging, update chaining, and valid-time bounding.

## Security and Privacy

ChronoGraph DB is a local embedded systems library with zero external dependencies, no network sockets, and no third-party libraries. All data resides in locally managed binary files. Memory safety is guaranteed by strict Java type safety and explicit bounds checking.

## Performance Characteristics

- **Write Throughput**: Append-only sequential disk writes bypass random I/O overhead.
- **Read Latency**: In-memory temporal indexes provide $O(\log N + K)$ lookup performance where $K$ is the number of historical versions.
- **Concurrency**: Java 21 virtual threads allow millions of concurrent reader tasks to evaluate snapshot pathfinding queries with minimal OS thread context-switching overhead.

## Limitations

- Maximum log file size is constrained by available disk storage; automated background compaction is basic.
- Single-node embedded architecture without distributed consensus clustering.
- In-memory index structures must fit within available JVM heap memory.
