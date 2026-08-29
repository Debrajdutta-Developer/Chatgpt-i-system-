# Autonomous Systems Software Factory

This repository runs a zero-touch software factory that discovers a difficult systems-engineering problem, builds a real runnable implementation, validates the implementation with factory-owned compile/test commands, repairs bounded failures, quality-gates the result, records evidence, and publishes successful releases as standalone GitHub repositories.

The target is **not ordinary CRUD/SaaS apps**. The target is engineering-heavy work in the spirit of a Java mini Redis, private search/index engine, small database engine, safe local learning/evaluation agent, C compiler/interpreter, message queue, parser, VM, storage engine, scheduler, or similarly deep infrastructure project.

## Daily lifecycle

```text
DISCOVER HARD SYSTEMS IDEA
  -> DEDUPLICATE
  -> SELECT LANGUAGE/PROFILE
  -> GENERATE REAL ENGINE + CLI/SERVER + TESTS
  -> STATIC SAFETY/DEPTH GATE
  -> COMPILE / BYTE-COMPILE
  -> UNIT TEST
  -> INTEGRATION TEST
  -> REPAIR (BOUNDED)
  -> 95/100 QUALITY GATE
  -> PROMOTE
  -> COMMIT
  -> STANDALONE REPOSITORY
  -> END-TO-END REPORT
```

The scheduled GitHub Actions workflow runs at **03:00 UTC / 08:30 IST** and can also be started manually.

## Systems-first discovery

Gemini is instructed to reject calculators, converters, note apps, landing pages, thin dashboards, basic CRUD products, API wrappers and superficial AI chat UIs.

New candidates must be **HARD or EXTREME** and contain at least eight connected features. Preferred project families include:

- compiler / interpreter / bytecode VM
- database / storage / query engine
- cache / protocol server
- private search / inverted-index / ranking engine
- message queue / event log
- version-control or build-system internals
- parser / static analyzer
- safe local agent with explicit memory, evaluation and feedback
- scheduler / observability / data-structure engine

A project must implement its central mechanism itself instead of delegating the hard part to a cloud API or third-party package.

## Supported generation profiles

The factory currently chooses exactly one of three deterministic profiles based on the problem.

### Java 21 systems profile

Typical fit: mini Redis/cache server, protocol server, message broker, scheduler or concurrent state engine.

Generated code includes a substantial reusable `Core.java`, runnable `Main.java`, a dependency-free unit suite and an integration suite. Local loopback networking is allowed for a real server project; public-network actions are not.

Validation includes strict Java compilation plus both executable test suites:

```text
javac -Xlint:all -Werror ...
java -ea -cp build/classes factory.CoreTest
java -ea -cp build/classes factory.IntegrationTest
```

### C11 systems profile

Typical fit: compiler, interpreter, parser, VM, storage/page engine or low-level data structure implementation.

Generated code includes `engine.h`, substantial `engine.c`, a real CLI in `main.c`, unit tests and integration tests.

Validation uses strict portable C flags:

```text
cc -std=c11 -Wall -Wextra -Werror -pedantic ...
./build/test_engine
./build/test_integration
```

The final runnable application must compile too.

### Python 3.12 systems profile

Typical fit: private search engine, local indexing/ranking system, safe feedback-learning agent, query engine, analysis engine or complex developer tool.

Generated code uses the standard library only. The central implementation lives in `src/engine.py`; `src/cli.py` must be a real argparse CLI. Unit and integration suites are validated separately.

```text
python -m compileall -q src tests
python -m unittest discover -s tests -p 'test_engine.py' -v
python -m unittest discover -s tests -p 'test_integration.py' -v
```

## Real-engine gate

A pretty interface or large amount of code is not enough. New systems projects must include all of the following:

- at least eight declared connected capabilities
- a substantial central engine, parser, protocol, index, state machine, execution layer or equivalent mechanism
- a useful runnable CLI or local server
- real malformed-input and boundary handling
- at least eight meaningful checks in the unit suite
- at least eight meaningful checks in the integration suite
- tests that execute the same engine used by the product
- honest README architecture, invariants, algorithms, supported scope and limitations
- no fake benchmarks, fake test claims, TODOs, placeholders or hard-coded success paths
- no shell/subprocess execution, self-modifying code, credentials, trackers or autonomous public-network actions

A safe learning agent may update local memory, scores, ranking or policies from explicit user-provided feedback. It may not execute arbitrary code, change its own source or pretend to train a neural model that it does not actually implement.

## Quality gate

New systems releases require **95/100** and every critical validation must pass.

| Dimension | Points |
|---|---:|
| Real problem / use case | 10 |
| Systems architecture | 15 |
| Correctness / all critical checks | 25 |
| Unit testing | 12 |
| Integration testing | 13 |
| Reproducible build | 10 |
| Documentation | 10 |
| Feature depth | 5 |

A numerical score can never override a compile failure, unit-test failure or integration-test failure.

## Repair behavior

When generated code fails static validation, compilation or tests, the validator returns bounded failure evidence to Gemini. The factory regenerates the selected project with that feedback up to the configured repair limit. If the final attempt still fails, the cycle records `FAILED` and does not publish the project.

## Standalone repository publishing

After a systems project passes the factory gate, the workflow runs the language-specific verification again at the release boundary. Only then does it create a separate public GitHub repository and push the validated project, including its own `.github/workflows/ci.yml`.

Required repository secrets:

```text
GEMINI_API_KEY
FACTORY_GITHUB_TOKEN
```

Never commit or paste either secret into source or chat logs.

## Truthful status

Every cycle creates a JSON report under `reports/` containing candidate ideas, selected technology, validation profile, validation output, repair attempts, quality score, project path and publication state.

`final_status: SUCCESS` means the generated project passed the factory's technical gate. For AI releases, `end_to_end_status` becomes `SUCCESS` only after the standalone repository is actually created and pushed.

A passing project is a **real tested scoped implementation**, not a claim of complete Redis compatibility, full SQL compatibility, industrial C compiler completeness, frontier AI capability, proven market demand, security certification or unlimited production scale unless separate evidence genuinely proves those things.

## Repository layout

```text
factory/
  provider.py       systems discovery + language-specific generation contract
  validator.py      factory-owned compile/unit/integration validation
  evaluator.py      95/100 evidence-based quality gate
  orchestrator.py   cycle, repair, reporting and promotion
  planner.py        duplicate-aware selection
  builder.py        reviewed deterministic fallback projects
projects/           validated releases retained in the factory monorepo
reports/            machine-readable cycle evidence
factory-history.json
.github/workflows/daily-factory.yml
```

## Local factory checks

Factory tests do not require Gemini:

```bash
python -m unittest discover -s tests -v
```

Run one AI cycle:

```bash
GEMINI_API_KEY="..." python -m factory
```

Plan only:

```bash
GEMINI_API_KEY="..." python -m factory --dry-run
```
