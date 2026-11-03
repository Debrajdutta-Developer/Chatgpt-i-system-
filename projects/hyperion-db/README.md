# Hyperion DB

Hyperion DB is a robust C11 embedded transactional storage engine and relational database designed specifically for resource-constrained systems, embedded devices, and database architects requiring strict ACID guarantees without excessive synchronous disk write overheads. By implementing ARIES-style write-ahead logging with fuzzy checkpoints, page-level latching, fine-grained lock management with deadlock detection, and a Volcano-style execution engine, Hyperion DB delivers predictable performance and deterministic crash recovery.

## Architecture

Hyperion DB is structured into modular layers that cleanly separate transaction semantics, indexing, buffer management, physical execution, and persistent storage:

1. **Storage and Buffer Pool Manager**: Manages fixed-size database pages in memory using a clock-sweep replacement algorithm and frame pinning. Handles asynchronous flushing and coordinates with the write-ahead logger.
2. **Write-Ahead Logging (WAL) and Recovery Subsystem**: Implements ARIES (Algorithms for Recovery and Isolation Exploiting Semantics) recovery. It processes Analysis, Redo, and Undo phases to guarantee atomicity and durability across abrupt power interruptions.
3. **Lock Manager**: Provides granular table, page, and tuple locks. Includes a robust wait-for graph deadlock detection mechanism that automatically aborts victim transactions upon cycle formation.
4. **B+Tree Index Manager**: Maintains on-disk and in-memory index structures supporting key-value lookups, range scans, and concurrent node splitting/merging.
5. **SQL Lexer, Parser & Optimizer**: Translates SQL statements (such as CREATE TABLE, INSERT INTO, SELECT, BEGIN, COMMIT, ROLLBACK) into relational algebra ASTs and optimizes execution plans using cost-based statistics.
6. **Volcano-Style Execution Engine**: Executes physical operators including Sequential Scans, Index Lookups, and Nested-Loop Joins via a pull-based iterator model.

## Algorithms and Data Structures

- **Clock-Sweep Page Replacement**: Used in the buffer pool manager to approximate Least Recently Used (LRU) behavior with minimal overhead, avoiding synchronization bottlenecks.
- **ARIES Recovery Protocol**: Utilizes log sequence numbers (LSNs), dirty page tables (DPT), and transaction tables. Redo runs forward from the checkpoint analysis phase; Undo rolls back loser transactions in reverse chronological order.
- **Wait-For Graph Deadlock Detection**: Dynamically tracks resource wait dependencies among active transactions and detects cycles using depth-first search.
- **B+Tree Balanced Search Trees**: Organizes keys with branching factors optimized for block-aligned I/O operations.
- **Volcano Iterator Model**: Standard `open()`, `next()`, `close()` protocol streaming tuples through relational operators.

## Invariants

1. **Write-Ahead Logging**: No data page shall be written to permanent storage before its corresponding log record is flushed (WAL protocol).
2. **Page Pinning**: A page frame cannot be evicted by the clock-sweep algorithm while its pin count is greater than zero.
3. **Atomicity and Durability**: Committed transactions are guaranteed durable after recovery; uncommitted transactions present in the log at failure time are fully rolled back during the Undo phase.
4. **Lock Exclusivity**: Conflicting locks (e.g., Exclusive vs. Shared) cannot be acquired concurrently on the same lockable resource entity.
5. **Memory Safety**: All buffer allocations, AST nodes, and transaction descriptors are strictly tracked and freed, preventing memory leaks during execution and error handling.

## Build and Run

Hyperion DB requires a standard C11 compiler (GCC or Clang) and Make. To build the engine, test suites, and the interactive command-line interface:

```bash
make all
```

To run the database interactive shell:

```bash
./hyperion_cli
```

To execute the unit and integration test suites:

```bash
make test
```

## Testing

The repository includes comprehensive verification suites:

- `tests/test_engine.c`: Validates buffer pool operations, B+Tree insertions and lookups, ARIES log recording, lock acquisition, deadlock detection, and parser correctness.
- `tests/test_integration.c`: Executes full end-to-end relational workflows, concurrent multi-threaded stress tests, transaction rollback scenarios, and simulated abrupt crash recovery tests.

Both test suites enforce strict assertion counts and return non-zero exit codes on failure.

## Security and Privacy

Hyperion DB operates entirely within embedded memory spaces and local filesystems without network listeners or external dependencies. It relies on standard library memory management and implements strict bounds checking on all SQL parsing buffers and record structures to prevent buffer overflows and undefined behavior.

## Performance Characteristics

- **Memory Footprint**: Extremely compact, tunable via buffer pool sizing constants.
- **Concurrency**: Fine-grained locking reduces contention across independent tables and pages.
- **Recovery Time**: Bounded by checkpoint frequency and active transaction count at failure time through ARIES fast analysis and selective redo.

## Limitations

- String and integer literals are supported, but advanced types (floats, BLOBs, dates) are reserved for future extensions.
- Joins are limited to nested-loop evaluations over small-to-medium datasets.
- Maximum string lengths in SQL queries are fixed at 31 characters to optimize embedded stack usage.
