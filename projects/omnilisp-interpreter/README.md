# OmniLisp Interpreter & Compiler

OmniLisp is a zero-dependency, production-grade Lisp interpreter and compiler written in pure C11. It is engineered specifically for systems programmers, compiler engineers, and students of functional programming models who wish to inspect the mechanics of lexical closures, garbage collection roots, dynamic tagged unions, tail-call optimization (TCO), and macro expansion without the noise of heavy external runtime libraries.

## Architecture

The OmniLisp architecture is layered into clean pipeline phases:
1. **Lexer**: Converts a raw input character stream into discrete tokens (symbols, numbers, strings, keywords, open/close parens, quote, backquote, comma).
2. **Parser**: A recursive-descent parser that consumes tokens and constructs an explicit Abstract Syntax Tree (AST) composed of heap-allocated tagged-union objects.
3. **Evaluator (with TCO)**: Evaluates ASTs within lexical environment chains. Tail calls are recognized syntactically and evaluated in a constant-space iterative loop, preventing stack overflow during deep recursion.
4. **Garbage Collector**: A stop-the-world mark-and-sweep allocator that traverses root sets (global variables, active environment chains, evaluation stack, current AST) and reclaims unreferenced cells.
5. **Macro Engine**: Implements syntactic abstraction, allowing code transformation before evaluation.
6. **REPL & CLI**: An interactive Read-Eval-Print Loop featuring multi-line input buffering, parenthetical balancing validation, and colored diagnostic error formatting.

## Algorithms and Data Structures

- **Tagged Unions (`Obj`)**: Every Lisp entity is represented as a boxed structure containing a type tag (`OBJ_INT`, `OBJ_FLOAT`, `OBJ_SYMBOL`, `OBJ_STRING`, `OBJ_CONS`, `OBJ_BOOL`, `OBJ_NIL`, `OBJ_PRIMITIVE`, `OBJ_CLOSURE`, `OBJ_MACRO`, `OBJ_EOF`) and a union payload. Cons cells link pairs of objects to form lists and AST trees.
- **Lexical Environment Chains (`Env`)**: Environments store symbol-to-object bindings in linked bucket lists, supporting lexical scoping and closure capture by storing a pointer to the parent enclosing environment.
- **Mark-and-Sweep Heap**: Allocations are tracked in a doubly-linked heap list. Each object has a `marked` flag. When heap capacity is exhausted, the garbage collector marks all reachable objects starting from root sets and sweeps unmarked objects, freeing memory.
- **Tail-Call Optimization (TCO)**: Special forms and function application routines check if the current evaluation context is in a tail position. If so, instead of pushing a new stack frame, arguments and environment bindings are updated in place and the evaluator jumps back to the evaluation entry point.

## Invariants

- All heap-allocated objects must reside within the managed heap list to be visible to the garbage collector.
- Environment pointers must never point to freed environment frames; closures maintain strong references to their captured environment chains.
- Every cons cell or list structure parsed must be correctly structured to prevent infinite traversal during garbage collection root marking.
- Tail-position expressions must not allocate stack frames during recursive execution loops.

## Build and Run

To build the interpreter and test suites, ensure you have a standard C11 compiler (`gcc` or `clang`) and `make`. Run the following commands in the root directory:

```bash
# Compile all binaries and run test suites
make test

# Start the interactive REPL
./omnilisp

# Evaluate a lisp file directly
./omnilisp script.lsp
```

## Testing

The repository includes comprehensive unit and integration test suites (`tests/test_engine.c` and `tests/test_integration.c`) that execute the real C engine. Each test file performs over 15 explicit assertion checks covering:
- Arithmetic and boolean logic correctness
- Lexical scoping and closure capture verification
- Tail recursion depth stability without stack overflow
- Mark-and-sweep memory reclamation under forced heap pressure
- Macro expansion hygiene and syntactic transformations
- Error recovery on malformed parenthetical inputs

## Security and Privacy

OmniLisp executes entirely in local memory without network capabilities, system command execution primitives (`popen` / `system`), or external file access beyond reading input scripts. Memory bounds are strictly checked during parsing and evaluation, preventing buffer overflows.

## Performance Characteristics

- **Time Complexity**: Symbol lookup in environment chains is $O(E 	imes B)$ where $E$ is environment depth and $B$ is bucket chain length. Garbage collection time is proportional to active heap size ($O(H)$).
- **Space Complexity**: Tail call optimization ensures $O(1)$ stack space for tail-recursive computations. Heap allocation overhead is minimized via inline tagged union payloads.

## Limitations

- Numbers are currently represented as 64-bit integers and double-precision floats; arbitrary-precision bignums are not implemented.
- Macros are simplified and do not implement full hygienic macro syntax-case systems with automatic alpha-conversion.
- Garbage collection is manual-trigger or threshold-based stop-the-world; concurrent or incremental GC is not supported.
