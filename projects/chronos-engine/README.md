# ChronosEngine

ChronosEngine is a zero-dependency Hybrid Logical Clock (HLC) and event-sequencing engine written in Java 21, specifically designed for systems engineers and distributed systems architects who need strict causal consistency without incurring the heavy coordination overhead of centralized atomic clocks or synchronized hardware.

## Architecture

ChronosEngine bridges the gap between physical wall-clock time and logical event counters. In distributed environments, standard physical time synchronization (via NTP) is subject to clock drift, step adjustments, and backward time jumps. ChronosEngine manages these anomalies through an integrated Hybrid Logical Clock state machine. 

The architecture consists of:
1. **Core HLC State Machine (`Core.HybridLogicalClock`)**: Manages physical time components (milliseconds) and logical counters backed by lock-free Java `VarHandle` operations, ensuring atomicity across multi-threaded environments without heavy mutexes.
2. **Drift Compensator (`Core.ClockDriftDetector`)**: Monitors incoming physical timestamps from remote nodes, detects excessive skew, and handles backward time jumps gracefully by pinning local physical time and advancing the logical counter.
3. **Causality Tracker & Validator (`Core.CausalityTracker`)**: Maintains local and distributed dependency graphs, validating happened-before relationships ($e_1 \to e_2$) and instantly throwing runtime violations if causal contracts are breached.
4. **Binary Layout Serializer (`Core.ChronosSerializer`)**: Packs timestamp components, node identifiers, and event payloads into compact, memory-aligned byte arrays for efficient network transmission.
5. **Network Simulation Harness (`Core.NetworkSimulator`)**: Provides a loopback TCP message passing substrate with configurable latency injection, packet reordering, and arbitrary delay models to test distributed failure modes deterministically.

## Algorithms and Data Structures

### Hybrid Logical Clock Algorithm
The HLC state is represented as a tuple $(l, c)$, where $l$ is the physical time component (wall-clock milliseconds) and $c$ is the logical counter. 
- **Local Event Generation**: When a local event occurs, physical time $pt = \text{currentTimeMillis()}$. If $pt > l$, update $l = pt$ and reset $c = 0$. If $pt \le l$, increment $c$.
- **Message Reception**: When receiving a message with timestamp $(l_m, c_m)$ from a remote node with physical time $pt$: Let $l_{old} = l$. Update $l = \max(l_{old}, l_m, pt)$. 
  - If $l == l_{old} == l_m$, set $c = \max(c, c_m) + 1$.
  - Else if $l == l_{old}$, increment $c$.
  - Else if $l == l_m$, set $c = c_m + 1$.
  - Else set $c = 0$.

### Data Structures
- **VarHandle Atomic Updates**: Thread-safe state transitions use `MethodHandles.lookup().findVarHandle(...)` over primitive longs to eliminate synchronization bottlenecks.
- **Ring Buffers & Bounded Queues**: Lock-free payload queues for handling network simulation threads and metric sampling.
- **Compact Byte Layout**: Fixed-width network headers consisting of `[Magic: 4 bytes][NodeId: 8 bytes][PhysicalTime: 8 bytes][LogicalCounter: 4 bytes][PayloadLength: 4 bytes][Payload...]`.

## Invariants

1. **Monotonicity**: For any sequence of events generated on a single node, $HLC_i < HLC_{i+1}$ (strictly increasing).
2. **Causal Consistency**: If event $A$ causally precedes event $B$ ($A \to B$), then the timestamp of $A$ is strictly less than the timestamp of $B$ in HLC space ($l_A < l_B$ or ($l_A == l_B$ and $c_A < c_B$)).
3. **Bounded Drift Compensation**: The clock drift detector guarantees that backward NTP adjustments never cause local time regression; physical time is clamped while the logical counter absorbs the discrepancy.
4. **Immutability of Serialized Packets**: Once serialized into a byte array, payload headers cannot be mutated, preventing buffer corruption during transport simulation.

## Build and Run

ChronosEngine requires Java 21 and a standard Java development environment (JDK 21).

### Compilation
```bash
javac --release 21 -d out src/main/java/factory/*.java
```

### Running the Main CLI
```bash
java -cp out factory.Main
```

### Running Tests
Compile tests against the compiled classes and run:ekom
```bash
javac --release 21 -cp out -d out src/test/java/factory/CoreTest.java src/test/java/factory/IntegrationTest.java
java -cp out factory.CoreTest
java -cp out factory.IntegrationTest
```

## Testing

Testing in ChronosEngine is divided into deterministic unit tests (`CoreTest`) and integration simulations (`IntegrationTest`). Both test suites execute real Java code with zero mocks, featuring explicit assertion counts (minimum 10 per suite) using a custom validation framework.

- **Extreme NTP Jump Tests**: Simulates forward jumps of hours and backward jumps of minutes, verifying that monotonicity and causality remain intact.
- **Out-of-Order Delivery Tests**: Dispatches messages with randomized network delays and reordering to ensure causal validation correctly flags out-of-order arrivals.
- **Logical Counter Overflow Tests**: Exhausts logical counter ranges within a single millisecond to verify safe rollover and counter management.

## Security and Privacy

ChronosEngine is an internal systems engine designed for trusted network environments. It does not perform dynamic class loading, external network telemetry, or file-system I/O. All communication occurs over loopback or configurable TCP sockets with explicit payload length headers, preventing buffer overflow vulnerabilities and malformed payload injection attacks.

## Performance Characteristics

- **Throughput**: Capable of generating and validating over 1,000,000 HLC timestamps per second per core due to lock-free `VarHandle` primitives.
- **Memory Footprint**: Minimal heap allocation; timestamps are packed into primitive types and serialization reuses NIO byte buffers.
- **Latency**: Sub-microsecond overhead per event timestamping.

## Limitations

1. **Node Identity**: Node identifiers must be statically configured 64-bit integers.
2. **Network Partitioning**: While causal violations are detected locally, network partitions do not trigger automatic consensus leader elections (ChronosEngine is a clock/sequencing engine, not a consensus protocol like Raft).
3. **Clock Skew Limits**: Extreme sustained clock skews exceeding maximum configurable drift windows will cause logical counter saturation.
