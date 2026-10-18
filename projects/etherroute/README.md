# EtherRoute: Userspace Network Router and Transport Protocol Stack

EtherRoute is a high-performance, zero-dependency userspace network router and reliable transport protocol stack implemented entirely in Java 21. It provides deterministic packet routing, custom Ethernet/IPv4 packet parsing, longest-prefix match routing tables, non-blocking I/O multiplexing, sliding-window flow control, congestion management, and an integrated network impairment simulation harness.

## Architecture

The architecture of EtherRoute is strictly layered to mimic the TCP/IP stack while operating entirely within user space over virtual loopback channels and non-blocking socket primitives. At the lowest layer, the `VirtualInterface` simulation harness captures raw byte arrays representing Ethernet frames. The parsing engine (`PacketParser`) decodes these bytes into structured objects (`EthernetFrame`, `IPv4Packet`, `TcpSegment`) without external libraries or memory copying overhead.

The routing engine (`RoutingTable`) indexes subnet routes using a compressed binary trie structure, executing Longest-Prefix Match (LPM) lookups in logarithmic time relative to address bit-width. Packets that pass routing checks are handed to the transport layer (`TransportEngine`), which manages sliding-window sequence numbers, cumulative and selective acknowledgments, retransmission timers, and congestion control loops (slow start, congestion avoidance, and fast recovery).

The non-blocking event loop is driven by Java NIO `Selector` and `SocketChannel` constructs, permitting concurrent packet injection, delay emulation, and multi-client routing simulation without thread-per-connection scaling penalties.

## Algorithms and Data Structures

1. **Compressed Binary Trie (LPM Routing)**:
   The routing table stores network prefixes by walking the bits of the destination IPv4 address. Each node in the trie represents a bit (0 or 1). When looking up a destination IP, the engine traverses the trie and tracks the most specific matching prefix encountered, satisfying the Longest-Prefix Match requirement efficiently.

2. **Sliding-Window Flow Control & SACK**:
   The transport layer maintains a congestion window (`cwnd`) and receiver window (`rwnd`). Out-of-order segments are buffered in a jitter queue, and selective acknowledgments track receipt ranges to avoid redundant retransmissions during partial packet loss.

3. **Congestion Control State Machine**:
   Implements RFC-compliant Reno/NewReno congestion control states: Slow Start (exponential growth of `cwnd`), Congestion Avoidance (linear growth), and Fast Recovery (halving `cwnd` upon triple duplicate ACKs and retransmitting missing segments immediately).

4. **Deterministic Impairment Queue**:
   Packets destined for impaired routes are held in a priority delay queue stamped with virtual release timestamps, allowing precise drop probability and jitter injection during testing.

## Invariants

- **Frame Boundary Safety**: Every packet parser must validate minimum length constraints (e.g., Ethernet header >= 14 bytes, IPv4 header >= 20 bytes) before inspecting internal fields, throwing strict `IllegalArgumentException` on malformed inputs.
- **Routing Optimality**: LPM routing must always select the route with the longest matching prefix bitmask. If no route matches, packets are dropped or routed to the default gateway (0.0.0.0/0).
- **Transport Sequence Monotonicity**: Sequence numbers within a connection stream must increment monotonically according to payload byte length, wrapping correctly at $2^{32}-1$.
- **Window Bounding**: The effective transmission window can never exceed the minimum of the congestion window and the receiver advertised window (`min(cwnd, rwnd)`).

## Build and Run

EtherRoute requires Java 21. Compile and execute the test suites and CLI runner using standard JDK tools:

```bash
# Compile all classes
javac -d bin src/main/java/factory/*.java src/test/java/factory/*.java

# Run the main server/CLI demo
java -cp bin factory.Main

# Run Unit Tests
java -cp bin factory.CoreTest

# Run Integration Tests
java -cp bin factory.IntegrationTest
```

## Testing

The verification suite is divided into unit tests (`CoreTest`) and integration tests (`IntegrationTest`), containing over 20 distinct assertion checks. The tests inject corrupted Ethernet frames, overlapping subnet masks, out-of-order TCP segments, and dropped packets into loopback channels to verify state machine resilience under adverse network conditions.

## Security and Privacy

EtherRoute operates entirely in userspace over virtualized network buffers and loopback socket channels. It requires no elevated root privileges, raw network interface binding (libpcap/AF_PACKET), or external native libraries. All parsed packets are strictly validated against bounds checks, preventing buffer over-reads, integer overflows, and malformed packet injection exploits.

## Performance Characteristics

- **Zero-Copy Parsing**: Byte buffers are sliced and read directly without intermediate heap object allocation during packet ingestion.
- **O(K) Routing Lookups**: Trie traversal time is bounded by $K = 32$ bit operations per IPv4 address lookup.
- **Non-Blocking Scalability**: Single-threaded NIO event selectors handle thousands of concurrent virtual socket events efficiently.

## Limitations

- IPv6 support is limited to basic header parsing and dummy routing; advanced extension headers and neighbor discovery are not implemented.
- The transport layer operates over reliable loopback channels rather than true raw socket hardware.
- Congestion control omits advanced cubic or BBR algorithms in favor of standard TCP Reno.
