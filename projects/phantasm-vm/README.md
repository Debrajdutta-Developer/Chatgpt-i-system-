# Phantasm-VM: Zero-Dependency Time-Travel Execution Engine

Phantasm-VM is a deterministic, zero-dependency 32-bit register-based virtual machine and time-travel debugging engine written in standard C11. It provides systems software engineers and compiler developers with bi-directional execution capabilities, enabling precise forward and reverse step-back operations through page-level state snapshotting and instruction differential logging.

## Architecture

Phantasm-VM is structured around a modular, decoupled architecture consisting of several core subsystems:

1. **Virtual Instruction Set Architecture (ISA)**: A custom 32-bit register-based ISA featuring 16 general-purpose registers (`R0` through `R15`), a program counter (`PC`), a stack pointer (`SP`), a frame pointer (`FP`), and a processor status flag register (`FLAGS`). Opcodes are explicitly encoded into 32-bit words containing opcode tags, destination registers, source registers, and immediate constants or memory displacement values.
2. **Execution Pipeline & Time-Travel Engine**: Manages the forward interpretation loop and coordinates with the snapshot manager to execute deterministic step-back operations. When executing forward, the engine logs instruction deltas and checkpoints state at configurable intervals or upon request.
3. **Memory Subsystem**: A linear address space representing physical/virtual RAM for the VM, featuring page-level alignment checks, bounds enforcement, and an explicit stack guard boundary to prevent stack overflow anomalies.
4. **Snapshot & Differential Compression Engine**: Implements copy-on-write page-level diffing to store memory and register state histories compactly, allowing instantaneous reconstruction of previous execution states.
5. **Breakpoint & Watchpoint Subsystem**: Evaluates logical predicates against register values and memory addresses before executing instructions.
6. **Disassembler Subsystem**: Translates raw binary bytecode buffers into human-readable assembly mnemonic strings for debugging and inspection.

## Algorithms and Data Structures

- **Register File & State Snapshot (`VMState`)**: Stores 16 general-purpose registers, program counter, stack pointer, frame pointer, processor flags, and execution cycle counters. Snapshots capture the complete register context and a differential log of modified memory pages.
- **Copy-on-Write Memory Diffing**: Instead of duplicating the entire linear memory space on every instruction, the snapshot engine records memory modifications as delta records containing physical address offsets, old values, and new values. Reversing an instruction applies the inverse delta record.
- **Instruction Decoding & Execution**: Uses a switch-dispatch interpreter loop over explicitly bit-masked 32-bit words. Arithmetic, logic, branch, memory load/store, and stack operations maintain strict determinism by forbidding non-deterministic system interactions.
- **Breakpoint Predicate Evaluation**: Evaluates conditional expressions (e.g., register equality, memory range checks) in $O(1)$ time during the instruction fetch-decode cycle.

## Invariants

- **Memory Safety**: All memory reads and writes are bounded strictly by the configured memory size (default 64KB). Out-of-bounds or unaligned memory accesses immediately trigger `VM_ERR_OUT_OF_BOUNDS` or `VM_ERR_UNALIGNED` without corrupting host memory.
- **Stack Bounds**: The stack pointer (`SP`) must remain within the designated stack segment. Exceeding stack boundaries triggers `VM_ERR_STACK_OVERFLOW`.
- **Deterministic Replay**: Executing a program forward for $N$ steps, then stepping backward $N$ steps, guarantees that the resulting register file and memory state are bit-exact to the initial state.
- **Zero External Dependencies**: The engine relies solely on standard ISO C11 libraries (`<stdint.h>`, `<stdbool.h>`, `<stdlib.h>`, `<string.h>`, `<stdio.h>`).

## Build and Run

Phantasm-VM can be built using any standard C11 compiler (GCC, Clang, MSVC) with strict warning flags enabled.

```bash
# Compile the engine and test suites
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c src/main.c -o phantasm
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c tests/test_engine.c -o test_engine
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c tests/test_integration.c -o test_integration

# Run automated test suites
./test_engine
./test_integration

# Run CLI interpreter / time-travel debugger
./phantasm
```

## Testing

The repository includes comprehensive unit and integration test suites located in `tests/test_engine.c` and `tests/test_integration.c`. Each suite exercises at least 10 explicit assertion checkpoints covering normal execution paths, malformed bytecode frames, memory boundary violations, unaligned memory access, stack overflows, breakpoint evaluation, disassembly correctness, and bit-exact reverse step-back fidelity.

## Security and Privacy

Phantasm-VM operates entirely within a sandboxed virtual memory space. It performs no dynamic linking to untrusted plugins, executes no system calls, reads no external files unless loaded explicitly via the CLI bytecode buffer API, and spawns no subprocesses. All memory allocations are bounded, managed, and freed cleanly.

## Performance Characteristics

- **Time Complexity**: Instruction decoding and execution operate in $O(1)$ time per instruction. Snapshot diffing operates in $O(M)$ time where $M$ is the number of modified bytes in an instruction window.
- **Space Complexity**: Memory footprint is bounded by the configured VM RAM size plus the snapshot history ring buffer size. Differential snapshotting reduces memory overhead by storing only modified page deltas.

## Limitations

- The virtual machine provides a simulated 32-bit register architecture without hardware floating-point support or operating system system-call primitives.
- Snapshot history depth is bounded by configured ring buffer capacity; extremely long execution traces without intermediate checkpoints may exhaust snapshot memory buffers if history limits are exceeded.
