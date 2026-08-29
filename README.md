# Autonomous Software Factory

This repository runs a zero-touch software-factory cycle that discovers a distinct useful problem, builds a substantial self-hosted product, validates real behavior, repairs bounded failures, quality-gates the result, records evidence, and publishes successful releases as standalone GitHub repositories.

The factory is designed to reject pretty-but-fake demos. A generated product is not releasable merely because files exist or the UI renders.

## Daily lifecycle

```text
DISCOVER -> DEDUPLICATE -> SELECT -> ARCHITECT -> BUILD
         -> STATIC GATE -> DOMAIN TESTS -> API TESTS -> FRONTEND BUILD
         -> DOCKER BUILD -> REPAIR -> 95/100 QUALITY GATE
         -> PROMOTE -> COMMIT -> STANDALONE REPO -> FINAL REPORT
```

The scheduled GitHub Actions workflow runs at **03:00 UTC / 08:30 IST** and can also be started manually.

## Production product profile

New AI-selected products use a fixed production-style profile:

- React + TypeScript frontend
- Node.js REST backend
- SQLite persistence through Node's built-in `node:sqlite`
- pure domain/business-logic layer
- database/schema layer
- real HTTP API integration tests
- domain unit tests
- Vite production frontend build
- multi-stage Docker image
- per-product GitHub Actions CI
- standalone public GitHub repository after every successful cycle

The factory chooses problems that can remain genuinely useful when run locally or self-hosted. It does not invent paid-service integrations or pretend that unavailable third-party systems are connected.

## Real-product gate

Gemini must propose at least six connected product capabilities. Toy calculators, trivial converters, basic CRUD lists, thin dashboards, note apps, landing pages and superficial demos are explicitly rejected at discovery/build time.

Generated source must contain a substantial domain engine, server-side validation, persistent SQLite operations, real same-origin `/api/...` frontend calls, multi-step workflows, and honest error/empty/loading states. The product must include both domain tests and API integration tests. Tests execute the same domain/server code used by the product.

The dependency manifest, TypeScript/Vite configuration, Dockerfile and product CI workflow are owned by trusted factory code rather than Gemini. Generated source cannot choose the validation commands.

## Validation

For the production full-stack profile the factory performs all of these before release:

```text
static architecture/security checks
npm install --ignore-scripts --no-audit --no-fund
npm test
npm run build
docker build
```

The static gate checks required architecture, minimum implementation depth, React/API wiring, `node:sqlite` persistence, exported testable HTTP server, domain/API test structure, documentation, dependency allowlists, unfinished markers, dangerous dynamic/system behavior, and unexpected public-network URLs.

The Node test suite must exercise domain behavior and a real ephemeral local HTTP server backed by a temporary SQLite database. A build/test/container failure is a critical failure; numerical quality points cannot override it.

A failed AI build can be regenerated with validation feedback up to the configured repair limit. If the final attempt still fails, the cycle reports `FAILED` and the project is not published.

Reviewed deterministic Python fallback products retain their own controlled unittest/compile validation path.

## Quality gate

Production AI releases require **95/100** plus every critical validation to pass.

| Dimension | Points |
|---|---:|
| Real problem/use case | 10 |
| Production architecture | 15 |
| Correctness / critical validation | 20 |
| Functional domain + API testing | 20 |
| Production frontend build | 10 |
| Container build | 5 |
| Documentation | 10 |
| Security hygiene | 5 |
| Feature depth | 5 |

A project with fake controls, disconnected UI logic, failed tests, failed build, failed Docker image, inadequate architecture, or fewer than six substantial features cannot be released as a production-profile success.

## Standalone repository publishing

The main factory repository keeps the project, history and machine-readable report. After a successful production gate, the workflow re-runs the release-boundary tests and creates a separate public repository named after the project slug.

Required repository secrets:

```text
GEMINI_API_KEY
FACTORY_GITHUB_TOKEN
```

Never commit either value. `GEMINI_API_KEY` is supplied only to the Gemini factory step. `FACTORY_GITHUB_TOKEN` is supplied only to the standalone publishing step.

The workflow currently starts with `gemini-3.5-flash` and rotates through configured stable fallback models when a retryable provider failure occurs.

## Repository layout

```text
factory/
  orchestrator.py      cycle coordination and truthful reporting
  planner.py           discovery + duplicate-aware selection
  provider.py          Gemini boundary + trusted production scaffold
  builder.py           reviewed deterministic fallback blueprints
  validator.py         allowlisted static/functional/build/container validation
  evaluator.py         95-point production quality gate
  history.py           duplicate evidence + atomic history writes
  publisher.py         safe promotion/publication state
  config.py, models.py configuration and typed records
projects/              validated products retained in the factory monorepo
reports/               machine-readable cycle evidence
factory-history.json   successful release history
.github/workflows/     scheduled/manual automation
```

A generated standalone full-stack product contains a React/TypeScript `src/`, Node/SQLite `server/`, `tests/`, `package.json`, Vite/TypeScript configuration, Dockerfile, project metadata, README and its own `.github/workflows/ci.yml`.

## Local factory commands

Run trusted factory tests without requiring Gemini:

```bash
python -m unittest discover -s tests -v
```

Run one AI cycle:

```bash
GEMINI_API_KEY="..." python -m factory
```

Plan without generation/publication:

```bash
GEMINI_API_KEY="..." python -m factory --dry-run
```

## Truthful status

Every attempted cycle writes JSON evidence under `reports/`, including candidate ideas, rejections, selected architecture, validation commands/results, repair attempts, quality scores, project path, publication state and known limitations.

`final_status: SUCCESS` means the product passed the factory's production validation gate. AI releases then enter `standalone_status: PENDING`; the workflow changes `standalone_status` and `end_to_end_status` to `SUCCESS` only after the separate repository is actually created and pushed.

The factory does **not** claim that a technically validated product has proven market demand, real users, penetration-test certification, or unlimited production scalability. Those require evidence outside automated generation. It does claim only what its recorded tests/builds actually prove.
