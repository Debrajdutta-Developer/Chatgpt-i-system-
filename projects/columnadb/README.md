# ColumnaDB Engine

ColumnaDB is a high-performance, zero-dependency embedded columnar analytical database engine written in Java 21. Designed specifically for OLAP (Online Analytical Processing) workloads, ColumnaDB avoids the immense I/O waste of traditional row-oriented engines by storing data strictly by columns, packing homogenous values into dense binary blocks, applying advanced compression schemes (Run-Length Encoding and Dictionary Compression), and executing analytical queries using a vectorized Volcano-style iterator model.

## Architecture

The engine is divided into three primary tiers: Storage, Catalog, and Execution. 
- **Storage Tier**: Manages byte-level file serialization, columnar block chunking, checksum verification via CRC32, and compression codecs. Tables are partitioned into immutable segment files where each column is stored in its own dedicated byte range offset map.
- **Catalog Tier**: Maintains table schemas, data types (INT, LONG, DOUBLE, STRING), column statistics (min, max, distinct count, null count), and uniform histograms for cardinality estimation.
- **Execution Tier**: Features a custom SQL parser for SELECT, FROM, WHERE, GROUP BY, and JOIN statements; a Cost-Based Optimizer (CBO) that builds physical query trees using catalog histograms; and a vectorized operator runtime powered by Java 21 virtual threads.

## Algorithms and Data Structures

- **Vectorized Column Chunks**: Instead of row-by-row iteration, execution operators fetch fixed-size arrays (vectors) of primitives or dictionary-encoded integers, minimizing virtual method dispatch overhead.
- **Run-Length Encoding (RLE)**: Compresses repetitive sorted columns by storing pairs of `(run_length, value)`, drastically reducing storage and accelerating sequential scans.
- **Dictionary Compression**: Encodes high-cardinality string and integer columns by mapping unique values to dense integer IDs, storing an immutable dictionary array alongside compressed integer arrays.
- **Cost-Based Optimizer (CBO)**: Computes join orders and filter selectivities using dynamic programming and catalog statistics, picking the plan with the lowest estimated I/O cost.
- **Hybrid Hash Join & Sort-Merge Join**: In-memory physical join operators optimized for columnar memory buffers.

## Invariants

1. **Immutability of Segments**: Once written, columnar data blocks are immutable; updates occur via delta-merging or new segment appends.
2. **Type Safety**: Column blocks strictly enforce typed arrays. A schema definition once bound cannot be modified without explicit migration commands.
3. **Checksum Verification**: Every column block includes a trailing CRC32 checksum over its compressed payload. Any bit rot or corruption throws a storage integrity exception.
4. **Vector Batch Bounds**: Vector batches never exceed 1024 elements to maintain L1/L2 cache locality.

## Build and Run

ColumnaDB requires Java 21 and standard build tools (javac, jar).

```bash
# Compile the engine and tests
javac -d target src/main/java/factory/*.java src/test/java/factory/*.java

# Run the CLI / Demo Main entrypoint
java -cp target factory.Main

# Run Unit Tests
java -cp target factory.CoreTest

# Run Integration Tests
java -cp target factory.IntegrationTest
```

## Testing

ColumnaDB includes an extensive test suite verifying engine correctness across storage corruption, query parsing, cost-based optimization, RLE/Dictionary compression roundtrips, and vectorized joins. Both `CoreTest` and `IntegrationTest` implement standalone test drivers with over 10 independent assertion checks each.

## Security and Privacy

As an embedded local database engine, ColumnaDB does not open network sockets by default, minimizing the attack surface. It operates entirely within user-defined directories, utilizing standard file system permissions and explicit memory-mapped buffers. No telemetry or external network calls are performed.

## Performance Characteristics

- **Scan Throughput**: Millions of rows per second on single-attribute aggregation queries due to columnar layout skipping unreferenced columns entirely.
- **Compression Ratios**: 3x to 10x reduction on low-cardinality and sorted analytical datasets.
- **Memory Footprint**: Predictable, bounded memory usage configured via vector batch sizes and streaming block readers.

## Limitations

- **OLAP Focus**: Not optimized for high-frequency point updates or single-row transactional inserts (OLTP).
- **In-Memory & Local Files**: Distributed node coordination, distributed consensus, and cloud object store streaming are outside the current local single-node scope.
- **SQL Subset**: Supports core analytical SQL (Projection, Filter, Aggregate, Inner/Left Join), but excludes complex window functions and nested subqueries.
