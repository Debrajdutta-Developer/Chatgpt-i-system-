# OmniStream MQ

## Overview
OmniStream MQ is a high-performance, lightweight enterprise message queue and event streaming broker implemented from scratch in pure Java 21. Designed specifically for systems engineers and distributed infrastructure architects, OmniStream MQ addresses the gap between heavy, complex external cluster management systems and raw, unreliable socket protocols. It delivers multiplexed consumer subscriptions, explicit acknowledgment persistence, dead-letter queue routing, and deterministic message replay within a single standalone runtime.

## Architecture
OmniStream MQ is engineered with a modular, layered architecture:
1. **Network Layer (Reactor Pattern)**: Powered by Java's non-blocking `Selector` and `SocketChannel` multiplexing. It handles concurrent client connections over a custom binary framing protocol without thread-per-connection overhead.
2. **Protocol Parsing Engine**: Converts raw byte streams into structured command frames (`PRODUCE`, `CONSUME`, `ACK`, `NACK`, `HEARTBEAT`) with strict length prefixing and magic byte validation.
3. **Storage Engine (Append-Only Log)**: Organizes topics into partitions backed by segmented append-only log files (`.log`) and sparse index files (`.index`). Implements zero-copy-friendly read operations and configurable retention policies.
4. **Coordination Engine**: Manages consumer groups, dynamic partition rebalancing, and client heartbeats to track liveness and ensure fair, exclusive partition allocation.
5. **Reliability Subsystem**: Tracks message delivery attempts, handles explicit acknowledgments, routes poisoned messages exceeding retry thresholds to a Dead-Letter Queue (DLQ), and supports disk compaction.

## Algorithms and Data Structures
- **Segmented Log Index**: Each partition maintains a active segment and immutable historical segments. The index file stores fixed-width binary entries mapping logical message offsets to physical file byte positions, allowing $O(1)$ file seeking and $O(\log N)$ binary search for offset resolution.
- **Binary Wire Protocol**: Frame structure consists of `[Magic Byte: 0x53][Command: 1 Byte][Stream ID: 4 Bytes][Payload Length: 4 Bytes][Payload: N Bytes]`. This guarantees deterministic framing and prevents buffer over-reads.
- **Consumer Group Assignment Strategy**: Deterministic round-robin allocation of partition IDs across active group members ordered by client connection identifiers.
- **Retention Pruning Algorithm**: Background timer thread evaluating segment modification timestamps and total partition byte footprints against high-watermark thresholds, safely unlinking expired segment files.

## Invariants
- **Log Append Monotonicity**: Message offsets within a partition strictly increase monotonically; no offset is ever reused or reallocated.
- **Partition Exclusivity**: At any given time, a single partition within a consumer group is assigned to at most one active consumer member.
- **Delivery Safety**: Messages are retained in the active log until explicitly acknowledged or expired by retention policies; unacknowledged messages are eligible for redelivery after the visibility timeout.
- **Protocol Compliance**: Any frame exceeding maximum payload size bounds or containing invalid magic headers is immediately rejected with a connection termination.

## Build and Run
OmniStream MQ requires Java 21 SDK.

### Compile
```bash
javac -d target/classes src/main/java/factory/*.java
javac -cp target/classes -d target/test-classes src/test/java/factory/*.java
```

### Run Broker Server
```bash
java -cp target/classes factory.Main server 9092
```

### Run Unit Tests
```bash
java -cp target/classes:target/test-classes factory.CoreTest
```

### Run Integration Tests
```bash
java -cp target/classes:target/test-classes factory.IntegrationTest
```

## Testing
OmniStream MQ includes comprehensive test suites:
- **`CoreTest`**: Exercises individual engine components including binary wire framing, log segment appends, offset indexing, consumer group rebalancing, and DLQ routing.
- **`IntegrationTest`**: Simulates end-to-end multi-client TCP sessions, concurrent produce-consume workloads, abrupt client disconnections, partition failovers, and corrupted index file recovery.

## Security and Privacy
OmniStream MQ operates as a local-first infrastructure component. It does not perform telemetry, outbound network connections, or external credential validation. Authentication and encryption can be layered externally via TLS termination proxies. All data is stored locally within the designated broker data directory.

## Performance Characteristics
- **Throughput**: Capable of sustaining tens of thousands of messages per second per partition utilizing buffered NIO channel writes and direct memory byte buffers.
- **Latency**: Sub-millisecond append latency due to sequential disk I/O and zero unnecessary intermediate object allocations.
- **Memory Footprint**: Bounded memory utilization through configurable segment sizing and LRU index caching.

## Limitations
- Replication across multiple distinct physical machines is not built-in; OmniStream MQ focuses on single-node durability and local high-performance event streaming.
- Transactional multi-partition atomic commits are not supported; guarantees are provided at the individual partition log level.
