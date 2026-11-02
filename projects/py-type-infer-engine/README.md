# PyTypeInfer Engine

PyTypeInfer Engine is a zero-dependency static type inference and abstract interpretation engine written entirely in Python 3.12 standard library. It parses Python Abstract Syntax Trees (ASTs), constructs Control-Flow Graphs (CFGs), performs unification-based Hindley-Milner type inference, applies flow-sensitive type narrowing via guard analysis, tracks numeric and string intervals through abstract interpretation, resolves parametric polymorphism and generic type constraints, checks structural subtyping (protocols), detects dead-code paths, and emits deterministic diagnostic JSON reports.

## Architecture

The engine is organized as a pipeline that transforms source code strings through successive canonical representation layers:

1. **AST Parser**: Extracts lexical definitions, statement sequences, and expression trees using Python's built-in `ast` module.
2. **CFG Builder**: Deconstructs statement blocks into basic blocks, maps true/false branch jump targets, and establishes control flow edges.
3. **Type Environment & Unification Engine**: Manages lexical scopes, resolves type variables via union-find/unification algorithms, and enforces type consistency.
4. **Abstract Interpretation & Flow Analyzer**: Propagates value ranges (integer bounds, literal sets) and narrows types along conditional branch edges (e.g., `isinstance`, `is not None`).
5. **Structural Subtyping & Protocol Checker**: Validates that class definitions implement required method signatures for duck-typing verification.
6. **Dead-Code Detector**: Identifies unreachable basic blocks and dead conditional branches based on invariant conditions.
7. **Diagnostic Formatter**: Collects errors, warnings, and inferred types into a structured, line-and-column-indexed JSON report.

## Algorithms and Data Structures

- **Hindley-Milner Unification**: Uses disjoint-set (union-find) data structures with path compression to unify type variables (`TypeVar`) with concrete types (`Int`, `Str`, `Bool`, `List`, `Dict`, `Function`, `Tuple`, `Any`).
- **Worklist-based Abstract Interpretation**: Iterates over CFG basic blocks using a fixed-point iteration worklist algorithm to converge abstract value intervals (`Interval(min, max)`) and set constraints.
- **Flow-Sensitive Path Predicate Narrowing**: Evaluates AST conditions (`isinstance(x, T)`, `x is not None`) to narrow type assignments within the true and false branch contexts independently.
- **Cycle Detection**: Employs Tarjan's strongly connected components and cycle detection algorithms over dependency graphs to handle recursive functions and recursive type definitions safely without infinite recursion.

## Invariants

- **Soundness**: Every inferred type or range is a conservative over-approximation of runtime behaviors; if a type error is flagged, the expression is guaranteed not to evaluate cleanly under standard semantics.
- **Termination**: Fixed-point abstract interpretation terminates via monotonic interval widening and finite domain height constraints.
- **Idempotency**: Repeated runs of type inference on the same AST produce identical type environments and error lists.
- **Pure Functional AST Walk**: The AST is never mutated; all analysis state resides in explicit environment frames and CFG basic block attribute maps.

## Build and Run

PyTypeInfer Engine requires no external dependencies. Execute the engine directly via the command-line interface:

```bash
python3 src/cli.py path/to/source.py --format json --output report.json
```

Or run inline source analysis:

```bash
python3 src/cli.py --code "def add(a: int, b: int) -> int: return a + b"
```

## Testing

The project includes extensive unit and integration test suites exercising malformed ASTs, deeply nested control-flow branches, recursive function definitions, cyclic dependencies, and contradictory variable assignments.

Run the test suite using standard `unittest` discovery:

```bash
python3 -m unittest discover -s tests
```

## Security and Privacy

PyTypeInfer Engine performs static analysis entirely in-memory. It does not execute untrusted source code, invoke external network services, write arbitrary files outside explicit command-line flags, or execute dynamic subprocesses. It is completely safe for analyzing untrusted or hostile Python codebases.

## Performance Characteristics

- **Time Complexity**: $O(N \log N + E \cdot W)$ where $N$ is AST nodes, $E$ is CFG edges, and $W$ is the abstract interpretation worklist iteration factor.
- **Space Complexity**: $O(V + E)$ where $V$ is variables and scopes in the type environment and $E$ is CFG basic blocks.
- **Memory Footprint**: Lightweight standard-library collections ensure minimal RAM consumption even on codebases exceeding 10,000 lines.

## Limitations

- Dynamic features such as `exec`, `eval`, and metaclass monkey-patching are not statically resolved.
- Higher-order generic type inference is bounded to first-order parametric polymorphism.
- Exhaustive type narrowing is limited to basic boolean predicates, `isinstance`, and nullity checks.
