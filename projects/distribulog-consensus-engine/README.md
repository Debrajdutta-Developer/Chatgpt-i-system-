# DistribuLog

DistribuLog is a zero-dependency, local-consensus replicated write-ahead log built using Java 21 Virtual Threads. It implements a custom Raft-like consensus protocol over NIO-backed loopback TCP sockets, utilizing non-blocking channels and on-disk binary log structures with strict CRC32 validation.

## Architecture

The engine is structured around three primary subsystems located entirely within the `factory` package: the consensus engine (`Core`), the storage engine (binary WAL with CRC32 checks and snapshotting), and the networking/transport layer (nio socket server and client pools).

1. **Core Consensus State Machine (`Core.java`)**: Manages the Raft consensus lifecycle. Nodes transition between `FOLLOWER`, `CANDIDATE`, and `LEADER` states. It uses Java 21 Virtual Threads (`Thread.ofVirtual().unstarted(...)`) for background timers (heartbeats and randomized election timeouts) and RPC request processing without thread-pool exhaustion.
2. **Storage Subsystem**: Appends structured log entries (`LogEntry`) directly to a binary file format. Each record contains length prefixes, magic bytes, term, index, command payload, and a 32-bit CRC checksum. On startup, logs are replayed, validated, and truncated or committed based on quorum.
3. **Network Transport**: Uses `ServerSocketChannel` and `SocketChannel` configured in non-blocking mode. Custom wire protocols serialize AppendEntries and RequestVote RPCs directly into byte buffers with explicit framing.

## Algorithms and Data Structures

- **Raft Consensus & Log Matching**: Implements leader election, term-based ballot voting, and log replication. Every log entry carries an index and a term. Followers enforce the log matching property: a follower rejects `AppendEntries` if its log doesn't contain an entry at `prevLogIndex` matching `prevLogTerm`.
- **Binary WAL Format**: Data layout on disk:
  - `[Magic Byte: 1 byte]` (0xDB)
  - `[CRC32: 4 bytes]` (checksum of term, index, and payload)
  - `[Term: 8 bytes]` (long)
  - `[Index: 8 bytes]` (long)
  - `[Payload Length: 4 bytes]` (int)
  - `[Payload: N bytes]` (UTF-8 bytes)
- **State Machine Replication**: Commits entries once a majority of nodes acknowledge the write, applying commands sequentially to a thread-safe key-value map.
- **Log Compaction**: Snapshots state up to the last applied index, truncating historical log segments to disk while preserving term and index watermarks.

## Invariants

1. **Election Safety**: At most one leader can be elected in a given term.
2. **Leader Append-Only**: A leader never overwrites or truncates its own entries; it only appends new ones.
3. **Log Matching**: If two entries in different logs have the same index and term, then they store the same command and their logs are identical up to that index.
4. **Leader Completeness**: If a log entry is committed in a given term, that entry will be present in the logs of the leaders for all higher-numbered terms.
5. **State Machine Safety**: If a server has applied a log entry at a given index to its state machine, no other server will ever apply a different log entry for the same index.

## Build and Run

To compile and package the application using standard Java 21 tooling:

```bash
javac -d target/classes src/main/java/factory/*.java
javac -cp target/classes -d target/test-classes src/test/java/factory/*.java
```

To run the interactive server / CLI main entry point:

```bash
java -cp target/classes factory.Main
```

## Testing

The repository contains two comprehensive test suites with explicit assertions:

- **Unit Tests (`CoreTest.java`)**: Validates core components, CRC32 corruption detection, log entry serialization, state transitions, and snapshotting mechanisms.
- **Integration Tests (`IntegrationTest.java`)**: Spins up a 3-node cluster over local TCP sockets, tests leader election, RPC replication under simulated network drops, and Jepsen-style linearizable state updates.

To run tests:

```bash
java -cp target/classes:target/test-classes factory.CoreTest
java -cp target/classes:target/test-classes factory.IntegrationTest
```

## Security and Privacy

DistribuLog operates entirely locally over loopback interfaces. It does not load external libraries, make outbound network calls, read arbitrary host files, or handle authentication tokens. Memory and disk buffers are managed strictly within bounded sizes, preventing buffer overflow vulnerabilities or unbounded memory allocation during malformed packet reception.

## Performance Characteristics

- **Zero-Copy & NIO**: Uses non-blocking socket channels and direct byte buffer formatting to minimize GC pressure and context switching.
- **Virtual Threads**: Eliminates blocking thread pools. Each connection or background task runs on lightweight virtual threads, scaling cleanly to thousands of concurrent simulated cluster peers or client connections.
- **Throughput**: Capable of handling thousands of local log appends per second depending on disk fsync policy.

## Limitations

- **Single Machine Loopback**: Designed for local consensus verification and multi-core embedded replication; multi-datacenter WAN routing and TLS are outside the current scope.
- **In-Memory State Machine**: The applied state machine key-value store resides in memory, backed by the replicated WAL on disk.
- **Fixed Port Allocation**: Integration tests bind to pre-configured ephemeral or local test ports.
