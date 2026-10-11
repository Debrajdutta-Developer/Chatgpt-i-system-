# NexusQL Engine

NexusQL Engine is a zero-dependency, single-node relational database storage and execution engine written in Java 21. Designed for systems engineers and database architects, it implements core database subsystems from scratch without third-party libraries, including a custom SQL parser, a cost-based optimizer (CBO) using column histograms, a slotted-page binary storage manager, a lock-free page cache, Multi-Version Concurrency Control (MVCC), a vectorized Volcano-style execution engine, and a crash-recovery Write-Ahead Log (WAL).

## Architecture

The engine is organized into several distinct architectural layers:
1. **Lexer & Parser**: Converts raw SQL strings into tokens and builds a strongly-typed Abstract Syntax Tree (AST) using recursive descent parsing.
2. **Catalog & Statistics**: Manages table schemas, column metadata, and frequency histograms used by the query optimizer.
3. **Storage Engine**: Implements binary slotted pages supporting variable-length record packing, tombstone deletions, and direct heap byte buffer allocations.
4. **Buffer Pool & Cache**: A concurrent lock-free page cache managing fixed-size page frames with atomic reference tracking and clean/dirty eviction state.
5. **Write-Ahead Log & Recovery**: Guarantees durability by appending binary transaction records to a WAL before memory modification, with deterministic checkpoint replay.
6. **MVCC & Transactions**: Manages concurrency via read-views, version chaining, write locks, and transaction timestamps.
7. **Cost-Based Optimizer (CBO)**: Computes physical execution plans based on table scan costs, filter selectivity, and index or sequence scans.
8. **Vectorized Execution Pipeline**: Executes physical operators processing batches of tuples (vectors) to minimize virtual dispatch overhead and maximize CPU cache locality.

## Algorithms and Data Structures

- **Slotted-Page Layout**: Each binary page maintains a header (page ID, free space pointer, item count) followed by an array of slot pointers (offset and length) growing downward and record payloads growing upward. Variable-length records are packed dynamically.
- **Column Histograms**: Equal-width frequency histograms store frequency distributions per column, allowing the CBO to compute accurate selectivity estimates for equality and range predicates ($Selectivity = \frac{count}{total}$). u
- **Vectorized Batches**: Execution nodes process `VectorBatch` structures containing parallel arrays of primitive integers and reference slots, avoiding per-tuple virtual method invocation.
- **Lock-Free Cache Frames**: Page cache lookups utilize concurrent hash maps and atomic integer version stamps to coordinate concurrent thread access without global mutex bottlenecks.
- **MVCC Version Chains**: Records contain creation and expiration transaction identifiers, allowing concurrent readers to reconstruct valid point-in-time snapshots.

## Invariants

1. **Durability**: No committed transaction state can be lost after a crash; WAL records must be fsynced (simulated via explicit log flush markers) prior to page eviction.
2. **Page Integrity**: Slotted page free space pointers and item slots must never overlap or exceed page boundary limits ($HeaderSize + (SlotCount \times 4) \le FreeSpacePointer$).
3. **Isolation**: Snapshot reads never block concurrent writes, and write operations acquire exclusive locks on targeted record keys.
4. **Parser Soundness**: All accepted AST nodes must conform to the defined grammar; malformed syntax triggers immediate compilation exceptions.
5. **Vector Bounds**: Every `VectorBatch` must have a uniform row count across all participating column vectors.

## Build and Run

Prerequisites: Java 21 Development Kit.

To compile the engine and run the executable CLI harness:
```bash
javac -d target src/main/java/factory/*.java
java -cp target factory.Main
```

To run the unit test suite:
```bash
javac -cp target -d target src/test/java/factory/CoreTest.java src/main/java/factory/*.java
java -cp target factory.CoreTest
```

To run the integration test suite:
```bash
javac -cp target -d target src/test/java/factory/IntegrationTest.java src/main/java/factory/*.java
java -cp target factory.IntegrationTest
```

## Testing

NexusQL includes two comprehensive test suites (`CoreTest` and `IntegrationTest`), each executing multiple explicit assertion checks across storage, parsing, optimization, and recovery:
- `CoreTest`: Verifies lexer tokenization, AST generation, slotted-page record packing, page cache eviction, MVCC version visibility, and WAL replay.
- `IntegrationTest`: Exercises end-to-end SQL execution, complex query optimization with histograms, vectorized projections and selections, and recovery from corrupted binary storage.

## Security and Privacy

NexusQL operates as an in-process or local engine without external network dependencies, raw socket listeners, or dynamic class loading. All memory allocations are bounded within pre-allocated byte buffers, preventing heap exhaustion from oversized payloads. File I/O is restricted to configured local working directories.

## Performance Characteristics

- **Time Complexity**: Slotted page record lookup by slot index is $O(1)$; unindexed sequential table scans are $O(N)$; index and histogram-based selectivity estimations are $O(K)$ where $K$ is the number of histogram buckets.
- **Space Complexity**: Memory footprint is bounded by the configured page cache frame count and batch vector width. Slotted pages eliminate internal fragmentation through compaction upon record deletion.
- **Throughput**: Vectorized execution pipelines process tuples in blocks of 1024, maximizing L1/L2 CPU cache utilization and reducing branch mispredictions.

## Limitations

- Supports single-node deployment models without distributed consensus protocols.
- Implements a core subset of standard SQL (SELECT, INSERT, UPDATE, DELETE with WHERE clauses, projection lists, and simple joins).
- Transaction concurrency uses optimistic and pessimistic locking without distributed two-phase commit.
- Storage engine uses fixed-size memory-backed binary pages without background asynchronous disk flushing threads.
