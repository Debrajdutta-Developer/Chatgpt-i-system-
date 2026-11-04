# OmniParser Engine

## Architecture

The OmniParser Engine is a modular, high-performance C11 parsing and grammar analysis framework designed for systems engineers and compiler builders who require deterministic control over tokenization, parsing table generation, conflict resolution, and abstract syntax tree (AST) construction. Traditional parser generators emit opaque, difficult-to-debug state machine binaries or rely on heavy external runtime dependencies. OmniParser implements a completely transparent parsing pipeline directly in standard C11 without external dependencies.

The system is structured into five cohesive subsystems:
1. **Lexer Subsystem (`omni_lexer_t`)**: Performs regex-based and literal-based token matching against raw source streams, maintaining precise source coordinates (line and column numbers) and emitting structured `omni_token_t` records.
2. **Grammar & Set Calculator Subsystem (`omni_grammar_t`)**: Manages productions, non-terminals, and terminals. It computes FIRST and FOLLOW sets iteratively using fixed-point algorithms over compact bitsets and adjacency lists.
3. **Parser State & Table Generation Subsystem (`omni_parser_table_t`)**: Constructs LR(1) and LL(*) parse tables, analyzes shift-reduce and reduce-reduce conflicts, and exposes explicit conflict resolution hooks.
4. **Memory-Pooled AST Subsystem (`omni_pool_t`, `omni_ast_t`)**: Manages heap allocation overhead through fixed-size memory block pooling. All AST nodes are allocated within typed arenas, guaranteeing zero fragmentation and single-pass deallocation.
5. **Execution & Error Recovery Subsystem**: Executes syntax-directed translation, manages semantic action bindings, and executes panic-mode error recovery by synchronizing token streams upon lexical or grammatical faults.

## Algorithms and Data Structures

- **FIRST and FOLLOW Set Computation**: Implemented via iterative boolean matrix closure over grammar symbols. Non-terminals are indexed sequentially. The algorithm loops until no further set modifications occur, adhering to classical Dragon Book definitions.
- **LR(1) Item Set Construction**: Items comprise a production rule, a dot position, and a lookahead terminal. Sets of LR(1) items are canonicalized and stored in hash-indexed state graphs to prevent redundant expansion.
- **Memory Pooling**: The AST allocator uses a linked list of fixed-size blocks (`omni_pool_block_t`). Each block allocates a contiguous byte array from which node chunks are carved using internal pointer bumps, reducing allocator syscall overhead during deep recursive descent parsing.
- **Panic-Mode Error Recovery**: When an invalid token is encountered, the parser pops states until a state with a recovery transition on a designated synchronizing token (e.g., semicolons, closing braces) is found, discarding input tokens until alignment is restored.

## Invariants

1. **Memory Safety & Zero Leaks**: Every memory allocation through `omni_pool_alloc` or `malloc` has a corresponding explicit deallocation path via `omni_pool_free` or `omni_free_grammar`. Unit tests execute under strict leak checking.
2. **Deterministic State Transitions**: Given an identical grammar and token stream, the generated parser table entries, shift/reduce actions, and resulting AST topology are 100% deterministic.
3. **Token Coordinate Integrity**: Every token retains accurate line and column numbers throughout lexing, parsing, and error reporting.
4. **Finite State Termination**: FIRST/FOLLOW set iterations and LR(1) closure operations enforce hard iteration ceilings to prevent infinite loops on malformed circular grammars.

## Build and Run

To build the engine, CLI, and test suites, compile using standard C11 toolchains with strict warning flags:

```bash
# Compile the engine and CLI utility
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c src/main.c -o omniparser

# Run unit tests
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c tests/test_engine.c -o test_engine
./test_engine

# Run integration tests
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c tests/test_integration.c -o test_integration
./test_integration
```

## Testing

The project includes two comprehensive test binaries (`test_engine.c` and `test_integration.c`), each containing at least 10 rigorous assertions covering normal parsing flows, ambiguous grammars, left-recursion detection, malformed token streams, memory pool exhaustion recovery, and parse table conflict detection.

## Security and Privacy

OmniParser Engine processes purely local string inputs and grammar definitions. It does not perform network operations, access environment variables, execute shell commands, or interact with external file systems beyond standard input/output streams. Buffer overflows are mitigated through bounds-checked string copies and explicit length parameters.

## Performance Characteristics

- **Lexing Speed**: Processes tens of thousands of tokens per second through streamlined buffer scanning without backtracking regex engines.
- **AST Memory Footprint**: Allocates nodes in bulk memory blocks, eliminating per-node malloc overhead and maximizing CPU cache locality during AST traversals.
- **Parse Table Generation**: Scales gracefully for grammars up to hundreds of productions; LR(1) state generation is optimized using item signature hashing.

## Limitations

- The built-in regex matching engine supports literal strings, character classes, and basic repetition operators rather than full PCRE syntax.
- LR(1) table generation can experience state explosion on highly ambiguous or excessively large industrial grammars without grammar refactoring.
