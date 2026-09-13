# ByteScript VM

ByteScript VM is a zero-dependency, high-performance, stack-based virtual machine and compiler implemented in pure C11. It features a custom lexer, a Pratt-style recursive-descent parser, a bytecode compiler, a stack-based execution engine, a mark-and-sweep garbage collector, and a built-in disassembler.

## Architecture

```
+------------------+      +-------------------+      +---------------------+
|   Source Code    | ----> |   Lexer/Parser    | ----> |  Bytecode Compiler  |
+------------------+      +-------------------+      +---------------------+
                                                                |
                                                                v
+------------------+      +-------------------+      +---------------------+
| Garbage Collector| <--- |  Execution Engine | <--- |    Compiled Chunk   |
+------------------+      +-------------------+      +---------------------+
```

ByteScript VM is structured into several distinct subsystems:
1. **Lexer & Parser**: Tokenizes the input source string and parses it using a Pratt parser (top-down operator precedence) to handle expressions, statements, and declarations.
2. **Compiler**: Translates the parsed constructs directly into a dense stream of bytecode instructions, managing local variable scopes, constant pools, and jump offsets.
3. **Virtual Machine (VM)**: A stack-based execution engine that maintains an operand stack, a call stack of activation frames, and a global variable symbol table.
4. **Garbage Collector (GC)**: A deterministic mark-and-sweep garbage collector that traces roots from the VM's operand stack, call frames, and global variables to reclaim unused heap-allocated objects (strings, functions, and native bindings).
5. **Disassembler**: Translates raw bytecode instructions back into human-readable assembly mnemonics for debugging and verification.

## Algorithms and Data Structures

### Pratt Parsing
Expression parsing is driven by a Pratt parser, which maps token types to prefix and infix parsing functions with associated precedence levels. This avoids deep recursive-descent call stacks for complex expressions and cleanly handles operator precedence and associativity.

### Mark-and-Sweep Garbage Collection
The GC operates in two phases:
1. **Mark Phase**: Recursively traces all reachable objects starting from the root set (the VM's operand stack, active call frames, and global variables). Reachable objects have their `is_marked` flag set to `true`.
2. **Sweep Phase**: Iterates through the linked list of all allocated objects. Unmarked objects are freed, and marked objects have their `is_marked` flag reset to `false` for the next cycle.

### Stack-Based Execution
The VM uses an explicit operand stack for expression evaluation and a call stack of `CallFrame` structures to manage function calls. Each `CallFrame` tracks the executing function, its instruction pointer (`ip`), and its slots on the operand stack.

## Invariants

- **Stack Safety**: The operand stack pointer must never underflow below the current frame's base slot or overflow beyond `STACK_MAX`.
- **GC Safety**: Any newly allocated object must be pushed onto the operand stack or otherwise anchored to the root set before any operation that could trigger a garbage collection cycle, preventing premature reclamation.
- **Memory Ownership**: The VM is the sole owner of all heap-allocated objects. Freeing the VM guarantees that all allocated objects, chunks, and constant pools are completely deallocated.

## Build and Run

To build the project, use any standard C11 compiler (e.g., GCC or Clang):

```bash
gcc -std=c11 -Wall -Wextra -Werror -pedantic -O3 src/engine.c src/main.c -o bytescript
```

To run an interactive REPL:

```bash
./bytescript
```

To execute a source file:

```bash
./bytescript path/to/file.bs
```

## Testing

To compile and run the unit and integration tests:

```bash
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc src/engine.c tests/test_engine.c -o test_engine
./test_engine

gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc src/engine.c tests/test_integration.c -o test_integration
./test_integration
```

## Security and Privacy

- **No External Dependencies**: The project relies strictly on standard C library headers (`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<stdbool.h>`, `<stdint.h>`), eliminating supply-chain vulnerabilities.
- **Memory Isolation**: The VM executes bytecode within its own isolated virtual address space. It does not allow arbitrary pointer arithmetic or direct access to host memory.
- **No Network/Disk I/O**: The VM does not perform any network operations or unauthorized disk access, ensuring a secure sandbox environment.

## Performance Characteristics

- **Dense Bytecode**: Instructions are encoded as single-byte opcodes, followed by optional 8-bit or 16-bit operands, minimizing cache footprint.
- **Flat Operand Stack**: The operand stack is implemented as a contiguous array of `Value` structures, ensuring excellent cache locality during execution.
- **Amortized GC Overhead**: The garbage collector is triggered only when heap allocations exceed a dynamic threshold, minimizing execution pauses.

## Limitations

- **Single-Threaded**: The VM is designed for single-threaded execution and does not support concurrent execution of bytecode.
- **Fixed Stack Limits**: The operand stack and call stack have fixed maximum sizes (`STACK_MAX` and `FRAMES_MAX`), which limits the depth of recursion and expression complexity.
- **No JIT Compilation**: Bytecode is interpreted via a dispatch loop rather than compiled to native machine code.
