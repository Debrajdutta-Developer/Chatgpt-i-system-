# Autonomous Software Factory

This repository is a local-first, zero-touch pipeline that discovers, builds,
validates, quality-gates, archives, and—when run by its authenticated GitHub
workflow—publishes one small software product per successful cycle. Actual
subprocess exit codes determine success. A cycle may honestly finish as
`SUCCESS`, `NO_RELEASE`, or `FAILED`.

Env Sentinel, the repository's first completed product, is preserved separately
at [`projects/env-sentinel`](projects/env-sentinel/README.md).

## Architecture

```text
factory/                  lifecycle implementation
  orchestrator.py         one-cycle entry point and transactional coordination
  planner.py              candidate discovery and selection
  builder.py              reviewed, dependency-free product blueprints
  validator.py            allowlisted validation commands and failure classes
  evaluator.py            evidence-based quality gate
  history.py              semantic duplicate checks and atomic history writes
  publisher.py            atomic promotion and honest remote status
  config.py, models.py    configuration and typed lifecycle records
  prompts/                contract for a future pluggable model provider
projects/                 isolated, released products
reports/                  machine-readable cycle evidence
factory-history.json      metadata for successful releases
.github/workflows/        scheduled and manual automation
```

The factory currently uses three reviewed local blueprints rather than an AI
service. This makes the complete pipeline deterministic, testable offline, and
free from credential requirements. The provider boundary is documented so a
future model can propose content, but generated text will remain untrusted and
will never control validation commands.

## Run one cycle

Python 3.10 or newer is the only requirement.

```bash
python -m factory
```

One invocation inspects history, existing projects, and recent report paths;
considers multiple technical-product candidates; rejects semantic duplicates;
builds the first eligible candidate in `.factory-work/<cycle>/`; runs controlled
tests and compilation; evaluates the quality gate; and promotes the directory
only after it passes. The staging tree is cleaned whether the cycle succeeds or
fails.

Planning without generating or publishing is safe and explicit:

```bash
python -m factory --dry-run
```

Dry-run creates a report with `NO_RELEASE`, but cannot create a project, change
history, commit, or push. Local successful runs report
`LOCAL_COMPLETE_REMOTE_NOT_PUBLISHED`; they do not claim remote publication.

## Validation and repair

The validation engine runs only factory-owned allowlisted commands: Python
`unittest` and `compileall`. Each result records the argv, exit code, output
summary, criticality, and a failure class such as `TEST_FAILURE`,
`BUILD_FAILURE`, `DEPENDENCY_FAILURE`, or `NETWORK_FAILURE`. Arbitrary commands
from generated files, comments, READMEs, or fetched content are never executed.

A failing staged blueprint is discarded and recreated, then validated again,
with at most three repair attempts. A remaining critical failure produces
`FAILED`; the candidate is not promoted and history is not updated.

Run all current checks manually:

```bash
python -m unittest discover -s tests -v
(cd projects/env-sentinel && python -m unittest discover -s tests -v)
python -m compileall -q factory tests projects/env-sentinel
```

## Quality gate

A project needs at least 80/100:

| Dimension | Points |
|---|---:|
| Usefulness | 20 |
| Completeness | 20 |
| Correctness | 20 |
| Testing | 15 |
| Documentation | 10 |
| Security | 10 |
| Novelty | 5 |

Scores are evidence-based checks, not permission to ignore failures. Failed
critical tests/builds, absent source or documentation, or a sub-threshold score
always block release. If every candidate duplicates history or existing project
slugs, the cycle returns `NO_RELEASE` rather than manufacturing novelty.

## History and reports

`factory-history.json` records only successfully released projects, including
problem, purpose, category, keywords, major features, technology, validation,
quality, report, and real commit information when known. Writes use a temporary
file plus atomic replacement. Duplicate detection compares tokenized names,
problems, purposes, categories, features, and keywords using Jaccard similarity,
not just exact names.

Every attempted cycle writes JSON under `reports/` with its timestamp, selected
and rejected ideas, validation evidence, repair attempts, scores, status,
limitations, and publication state. Commit fields remain `null` unless a commit
is actually known.

## Automation and publishing

`daily-factory.yml` runs manually and daily at **03:00 UTC**, which is **08:30
Asia/Kolkata**, with a non-overlapping concurrency group. It validates the
factory, runs one cycle, validates all projects, stages only `projects/`,
`reports/`, and history, avoids empty commits, and pushes through the checkout's
built-in `GITHUB_TOKEN` authentication. Reports are uploaded even on failure.

The workflow requests only `contents: write`. No personal access token is used.
No repository secret is currently required. GitHub supplies `GITHUB_TOKEN`
automatically. If a future optional Gemini provider is implemented, its only
accepted key name will be `GEMINI_API_KEY`; the current factory does not read or
require it.

## Security model and limitations

- Secrets and dotenv files are ignored; secret values are never included in
  project reports.
- An exclusive local lock plus workflow concurrency prevents overlapping cycles.
- Validation commands are code-owned and allowlisted; project prose is untrusted.
- Blueprints use only the Python standard library, avoiding registry and package
  lifecycle risk.
- The finite reviewed blueprint catalog deliberately yields `NO_RELEASE` after
  all unique products have shipped. Expanding the catalog or adding a carefully
  sandboxed provider is required for indefinite generation.
- Static duplicate similarity is useful but not a semantic language model; its
  threshold is transparent and tested.
