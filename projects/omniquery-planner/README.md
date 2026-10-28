# OmniQuery Planner

## Overview
OmniQuery Planner is a zero-dependency relational query optimizer written in Java 21, designed specifically for database systems engineers and compiler researchers. Traditional database cost-based optimizers hide their search space exploration and rewrite heuristics inside monolithic commercial codebases, making it impossible to inspect how relational algebra trees are transformed into execution graphs or how join-order dynamic programming behaves under different cardinality estimators. OmniQuery Planner exposes this inner lifecycle entirely through clean, inspectable data structures, offering a Volcano/Cascades-style equivalence-class rule engine, a memoized search space container, a dynamic cost estimator, and a deterministic verification harness.

## Architecture
OmniQuery Planner is organized around a strictly layered relational optimization pipeline:

1. **Lexer and Parser**: Transforms textual SQL statements (`SELECT ... FROM ... WHERE ...`) into an explicit abstract syntax tree (AST) composed of `SqlNode` entities.
2. **Logical Analyzer**: Converts SQL AST nodes into initial relational algebra nodes (`RelNode`), handling project lists, filter predicates, and inner join graphs.
3. **Cascades/Volcano Memo Engine**: Groups equivalent relational expressions into equivalence classes (`Group`) inside a shared memoization container (`Memo`). Each group contains multiple physical or logical expressions (`GroupExpr`) sharing sub-plans to prevent exponential expression explosion.
4. **Rule Engine**: Applies transformation and implementation rules (e.g., Predicate Pushdown, Constant Folding, Join Reordering, Hash Join vs Nested Loop Join implementation) to generate new group expressions.
5. **Cost Model and Statistics**: Evaluates physical execution plans using table and column statistics histograms (Uniform and Equi-Depth) to assign realistic CPU and I/O cost scores.
6. **Property Enforcer**: Guarantees that required physical traits (such as sort orders or distribution keys) are satisfied before plan finalization.
7. **Plan Visualizer**: Renders hierarchical execution trees complete with operator types, estimated cardinalities, and cost metrics.

## Algorithms and Data Structures
- **Memoization Container (`Memo`)**: Uses equivalence classes (`Group`) mapped by integer IDs. Expressions (`GroupExpr`) store child group references, enabling structural sharing across alternative plans.
- **Dynamic Programming Join-Order Enumerator**: Implements bottom-up subset enumeration for binary inner joins, building optimal bushy and left-deep join trees while leveraging memoization to prune sub-optimal join permutations.
- **Equivalence Rule Application**: Matches root operator types and triggers rule firings that inject newly derived expressions into existing or new memo groups.
- **Cost Estimation Heuristics**: Computes selectivity factors using predicate types (equality, range, conjunction) against histogram bucket boundaries, estimating output cardinalities to feed relational cost functions.

## Invariants
- Every `GroupExpr` in the `Memo` structure must reference valid `Group` IDs that exist within the current optimization session.
- Cost estimations must always yield non-negative finite real numbers.
- Relational expression transformations must preserve semantic equivalence (soundness invariant).
- Plan extraction must always yield a valid, executable physical plan if at least one implementation path is found.
- Fallback mechanisms must trigger cleanly without throwing unchecked runtime exceptions when confronted with malformed inputs or empty catalog statistics.

## Build and Run
Requirements: Java 21 SDK.

To compile the project:
```bash
javac -d bin src/main/java/factory/*.java
```

To run the CLI application:
```bash
java -cp bin factory.Main "SELECT name, age FROM users WHERE age > 30"
```

To run the unit test suite:
```bash
javac -d bin -cp bin src/test/java/factory/CoreTest.java
java -cp bin factory.CoreTest
```

To run the integration test suite:
```bash
javac -d bin -cp bin src/test/java/factory/IntegrationTest.java
java -cp bin factory.IntegrationTest
```

## Testing
The project contains rigorous test suites covering unit validation of the lexer, parser, rule engine, memo structure, and cost model, as well as integration suites feeding malformed relational expressions, cyclic join graphs, and zero-cardinality catalog statistics to verify robust optimizer fallback and cost bounds exceptions. Tests execute via standalone main methods with explicit check assertions.

## Security and Privacy
OmniQuery Planner operates entirely in-memory with zero external network connectivity, disk I/O persistence, or native library dependencies. It processes strings provided through CLI arguments or test fixtures and maintains no persistent state, making it inherently thread-safe and secure against injection vulnerabilities or unauthorized data access.

## Performance Characteristics
- **Time Complexity**: Exponential in the number of tables for exhaustive join enumeration, mitigated by memoization and dynamic programming pruning.
- **Space Complexity**: Bounded by the size of the `Memo` table and active group expressions.
- **Concurrency**: Immutable query structures and thread-local optimization contexts allow parallel optimization runs without global lock contention.

## Limitations
- Supports inner joins, basic selections (`WHERE a = b AND c > 10`), and projection lists; outer joins and subqueries are not yet implemented.
- Catalog statistics are maintained via simplified histograms rather than dynamic sampling.
- Rule engine uses a fixed iteration budget rather than a complete fixpoint convergence loop for transformation rules.
