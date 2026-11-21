# Kernel-Level Packet Fuzzer (k-packet-fuzz)

## Overview
`k-packet-fuzz` is a robust, high-performance C11 network protocol fuzzer engineered specifically for network protocol engineers and systems security researchers. The tool focuses on uncovering edge-case vulnerabilities, parser bugs, state confusion, and resource management flaws within kernel-space TCP/IP stack implementations. By crafting fully arbitrary Ethernet, IPv4, and TCP headers, applying deterministic or randomized mutation strategies, corrupting checksums, and tracking state machine progression across synthetic streams, `k-packet-fuzz` provides the deterministic control necessary to stress-test complex network parsing logic.

## Architecture
The project is structured into a clean, modular C11 library and a standalone command-line driver accompanied by extensive test suites:
- **`src/engine.h`**: Core definitions, protocol headers wrappers, configuration structures, mutation control flags, and public API function prototypes.
- **`src/engine.c`**: Core fuzzing engine implementation, packet template generation, robust checksum calculators (supporting standard IP header and pseudo-header TCP checksums), pseudo-random number generator routines seeded deterministically, state machine tracker, and logging subsystems.
- **`src/main.c`**: Command-line interface providing robust argument parsing, operational mode selection (fuzzing loops, dry-run template generation, pcap recording), and real-time execution statistics.
- **`tests/test_engine.c`**: Comprehensive unit tests validating checksum arithmetic, mutation bounds, bit-flipping integrity, and state transition matrices.
- **`tests/test_integration.c`**: Integration test suite verifying end-to-end packet crafting, lifecycle tracing, log serialization, and memory cleanup under error conditions.

## Algorithms and Data Structures
- **Pseudo-Random Number Generation**: Employs a linear congruential generator (LCG) with deterministic seed initialization, allowing exact test-vector replay without external dependencies.
- **Checksum Algorithms**: Implements RFC 1071 one's complement checksum for both IPv4 headers and TCP pseudo-header transport checksums, correctly handling odd-length payloads and byte ordering.
- **TCP State Machine Tracker**: Implements a lightweight finite state machine (FSM) tracking standard TCP states (`CLOSED`, `SYN_SENT`, `SYN_RECEIVED`, `ESTABLISHED`, `FIN_WAIT`, `CLOSE_WAIT`, `TIME_WAIT`) based on flag bit combinations (`SYN`, `ACK`, `FIN`, `RST`).
- **Mutation Engine**: Uses bitwise mask application, field boundary clamping, and targeted byte substitution across Ethernet source/destination addresses, IP identification fields, TTLs, TCP sequence/acknowledgment numbers, window sizes, and urgent pointers.

## Invariants
- **Memory Safety**: All packet buffers have strict capacity bounds (`KPF_MAX_PACKET_SIZE`), and helper routines consistently check pointer offsets before reading or writing.
- **Deterministic Replay**: Given an identical random seed and configuration structure, the engine will generate the exact identical sequence of malformed packets.
- **Checksum Correctness**: When mutation flags omit intentional corruption, all generated packets satisfy network protocol RFC checksum integrity requirements.
- **Error Isolation**: Failure conditions (such as invalid configuration parameters or buffer exhaustion) return explicit error codes without terminating the host process abruptly.

## Build and Run
Prerequisites: A standard POSIX-compliant environment with a C11 compiler (`gcc` or `clang`) and `make`.

To build all binaries and run tests:
```bash
make
```

To execute the unit and integration tests:
```bash
make test
```

To run the packet fuzzer in dry-run mode (generating 100 packets without live interface injection):
```bash
./kpf_fuzzer --dry-run --count 100 --seed 1337 --output fuzz_session.pcap
```

To clean build artifacts:
```bash
make clean
```

## Testing
The test framework is fully self-contained using standard assertions. Each test file (`tests/test_engine.c` and `tests/test_integration.c`) executes at least 10 independent verification checks, covering normal packet parsing, malformed boundary conditions, state transition matrices, checksum recomputation, and file persistence.
- Run `make test` to execute both test binaries.

## Security and Privacy
`k-packet-fuzz` runs locally and operates entirely within user-space memory buffers unless explicitly configured to bind to raw sockets. It does not transmit telemetry, access credentials, or execute external commands. When executing against live interfaces, ensure you have appropriate administrative privileges and authorization.

## Performance Characteristics
- **Zero-Copy Where Possible**: Operates directly on contiguous pre-allocated byte arrays, minimizing heap allocations during high-throughput fuzzing loops.
- **Low Latency**: Generates and mutates thousands of packet headers per second without blocking on I/O when writing to memory buffers or dry-run logs.

## Limitations
- Raw socket injection requires root/CAP_NET_RAW privileges on Linux systems; without privileges, the engine gracefully falls back to dry-run or loopback simulation mode.
- TCP state tracking is heuristic and does not maintain a full multi-flow connection table, focusing instead on individual packet stream transitions.
- Limited to Ethernet, IPv4, and TCP protocol layers in this release; UDP and ICMP extensions are left for future expansion.
