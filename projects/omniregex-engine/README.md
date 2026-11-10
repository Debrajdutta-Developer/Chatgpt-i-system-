# OmniRegex Engine

OmniRegex Engine is a rigorous, industrial-grade C11 regular expression compiler and matching subsystem designed specifically for systems engineers and compiler authors who require predictable, bounded latency and immunity to catastrophic backtracking (ReDoS). Standard regex engines based on backtracking (NFA execution with backtracking or PCRE-style recursive matching) are vulnerable to exponential time explosion when presented with ambiguous or heavily nested patterns combined with adversarial strings. OmniRegex solves this by compiling regular expressions into deterministic finite automata (DFA) through a strict pipeline: Lexer -> Recursive Descent Parser -> Abstract Syntax Tree (AST) -> Thompson NFA Construction -> Powerset Construction (NFA to DFA) -> Hopcroft State Minimization -> Bytecode / Transition Table Execution.

## Architecture

The OmniRegex compiler pipeline is structured around clean separation of concerns and strict memory ownership models:

1. **Lexer (`src/engine.c`)**: Scans raw regex patterns into discrete tokens (literals, dot wildcard, character classes, alternation `|`, concatenation, Kleene star `*`, plus `+`, optional `?`, and grouping parentheses).
2. **Parser (`src/engine.c`)**: Implements a Pratt / recursive descent parser adhering to operator precedence (Kleene/Plus/Optional > Concatenation > Alternation). It builds an explicit Abstract Syntax Tree (AST) in memory.
3. **Thompson NFA Construction (`src/engine.c`)**: Converts the AST into a Non-Deterministic Finite Automaton using Thompson's construction algorithm, handling epsilon transitions cleanly without recursive stack consumption during matching.
4. **Powerset Construction (`src/engine.c`)**: Transforms the NFA into a Deterministic Finite Automaton (DFA) using subset construction with explicit epsilon-closure calculations over bitsets.
5. **State Minimization (`src/engine.c`)**: Applies Hopcroft's partition-refinement algorithm to compress redundant DFA states, reducing memory footprint and transition lookup overhead.
6. **Execution Engine (`src/engine.c`)**: Executes the minimized DFA transition table in strict $O(n)$ time relative to input string length. It operates iteratively without recursion or stack growth, preventing stack overflow and ReDoS vulnerabilities entirely.

## Algorithms and Data Structures

- **AST Nodes**: Dynamically allocated structures representing literals, character classes, concatenations, alternations, and quantifiers.
- **NFA States and Transitions**: Array-based storage of state structures containing epsilon transition pointers and character-labeled transition tables.
- **Bitsets**: Fixed-size bitmaps (`uint64_t` arrays) used for efficient set operations during epsilon-closure computations and DFA state subset construction.
- **DFA Transition Tables**: Dense matrix mapping `(State, ASCII Character) -> NextState`, optimized for cache-friendly single-pass linear scanning.
- **Hopcroft's Minimization**: Partition refinement algorithm dividing states into distinguishable equivalence classes based on behavior across input alphabet symbols.

## Invariants

- **Memory Safety**: All allocations performed during compilation and matching are tracked and freed cleanly. No memory leaks under error or success paths.
- **Bounded Execution**: Matching time is strictly proportional to input length ($O(n)$). No recursive traversal or backtracking state saves occur during match execution.
- **Determinism**: Every compiled regex pattern resolves to a single deterministic DFA transition table.
- **Error Propagation**: Malformed regex patterns (unbalanced parentheses, invalid quantifiers, dangling alternation operators) yield clean, descriptive error codes without crashing or invoking undefined behavior.

## Build and Run

OmniRegex requires a standard C11 compliant compiler (GCC or Clang) and a POSIX environment or standard build tools. To compile the engine, CLI, and test suites with strict warnings enabled:

```bash
# Compile all binaries
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c src/main.c -o omni_regex
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c tests/test_engine.c -o test_engine
gcc -std=c11 -Wall -Wextra -Werror -pedantic src/engine.c tests/test_integration.c -o test_integration

# Run CLI matching
./omni_regex "a(b|c)*d" "abbbcd"

# Run unit and integration tests
./test_engine
./test_integration
```

## Testing

The repository includes comprehensive unit and integration test suites:

- **Unit Tests (`tests/test_engine.c`)**: Exercises lexing, parsing, AST construction, NFA generation, powerset construction, minimization, and error handling with at least 10 independent assertions.
- **Integration Tests (`tests/test_integration.c`)**: Validates complex regex patterns, malformed syntax rejection, adversarial ReDoS strings (e.g., `(a+)+$`), boundary conditions, and large input performance under strict memory constraints.

## Security and Privacy

OmniRegex is designed specifically to eliminate ReDoS vulnerabilities. Because it converts NFA structures into deterministic DFAs prior to execution, adversarial inputs containing ambiguous repetitions cannot trigger exponential backtracking or CPU starvation. All inputs are processed entirely in local memory without network calls or external file access.

## Performance Characteristics

- **Compilation Time**: Proportional to regex pattern length $O(m^2)$ due to subset construction and DFA minimization.
- **Matching Time**: Strictly $O(n)$ where $n$ is the length of the target string. Memory lookups per character are $O(1)$.
- **Memory Footprint**: Bounded by maximum DFA state limits (`MAX_DFA_STATES`), making it suitable for embedded and high-throughput systems.

## Limitations

- **Alphabet Support**: Designed for standard 8-bit ASCII characters (0-255).
- **Unicode**: Full UTF-8 multi-byte decoding is not natively handled at the character class level (characters are treated as individual byte streams).
- **Captured Groups**: Advanced backreferences (e.g., `\1`) are intentionally omitted as they destroy DFA determinism.
