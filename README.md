# Autonomous Software Factory

This repository runs a zero-touch software-factory cycle that can discover a new useful problem, build a small local-first app, validate it, repair failures within a bounded loop, quality-gate the result, record evidence, and publish only validated changes.

Env Sentinel, the first completed product, is preserved at [`projects/env-sentinel`](projects/env-sentinel/README.md).

## Daily lifecycle

```text
DISCOVER -> DEDUPLICATE -> SELECT -> BUILD -> VALIDATE -> REPAIR
         -> QUALITY GATE -> PROMOTE -> HISTORY/REPORT -> COMMIT/PUSH
```

The scheduled GitHub Actions workflow runs at **03:00 UTC / 08:30 IST** and can also be started manually.

## AI discovery and building

When `GEMINI_API_KEY` is configured, `factory/provider.py` uses Gemini to propose multiple distinct candidate problems. Existing `factory-history.json`, successful reports, and existing project slugs are screened before selection so the factory does not knowingly repeat an old project.

The selected AI idea is built as a dependency-free local browser application using only:

- `index.html`
- `style.css`
- `app.js`
- `README.md`
- generated `project.json` metadata

The provider is intentionally constrained. Generated text is **untrusted data**: it cannot choose shell commands, change the validator allowlist, access repository secrets, or claim that validation passed.

If no Gemini key is present during a local run, the factory falls back to the reviewed deterministic blueprint catalog. The scheduled GitHub workflow requires the key so scheduled releases use AI discovery instead of silently pretending to be AI-powered.

## Required GitHub secret

Create this repository Actions secret:

```text
GEMINI_API_KEY
```

The workflow supplies it only to the factory-cycle process. GitHub's built-in `GITHUB_TOKEN` is used for repository publishing; no PAT is required in source code.

The model can be changed with `GEMINI_MODEL`. The workflow currently sets:

```text
gemini-3.7-flash
```

## Validation

Validation commands are selected by trusted factory code, never by generated files.

Reviewed Python CLI products run controlled `unittest` and `compileall` checks. AI-generated browser apps run factory-owned static checks plus `node --check app.js`. The web validator requires complete local files, HTML references to local CSS/JS, meaningful file sizes, and rejects obvious placeholders and remote/dynamic behaviors such as `fetch()`, remote URLs, `eval()`, WebSocket use, and dynamic code injection.

A failed generated app may be regenerated with a short validation-error summary, up to the configured repair limit. If critical validation still fails, the cycle reports `FAILED` and does not promote the app.

## Quality gate

A release needs at least **80/100** and all critical validation must pass.

| Dimension | Points |
|---|---:|
| Usefulness | 20 |
| Completeness | 20 |
| Correctness | 20 |
| Testing/validation | 15 |
| Documentation | 10 |
| Security | 10 |
| Novelty | 5 |

A high numerical score never overrides a critical validation failure.

## Repository layout

```text
factory/
  orchestrator.py      cycle coordination
  planner.py           AI/fallback discovery and duplicate-aware selection
  provider.py          Gemini API boundary and generated-file contract
  builder.py           reviewed deterministic fallback blueprints
  validator.py         allowlisted validation
  evaluator.py         quality gate
  history.py           semantic duplicate checks + atomic history writes
  publisher.py         promotion/publication state
  config.py, models.py configuration and typed records
projects/              released products
reports/               machine-readable cycle evidence
factory-history.json   successful release history
.github/workflows/     scheduled/manual automation
```

## Local commands

Run the factory tests without requiring Gemini/network access:

```bash
python -m unittest discover -s tests -v
```

Run one local factory cycle:

```bash
GEMINI_API_KEY="..." python -m factory
```

Plan only, without creating or publishing a project:

```bash
GEMINI_API_KEY="..." python -m factory --dry-run
```

Validate Env Sentinel:

```bash
(cd projects/env-sentinel && python -m unittest discover -s tests -v)
```

## Reports and truthful status

Every attempted cycle writes JSON evidence under `reports/`, including candidate ideas, duplicate rejections, validation output, repair attempts, quality scores, final status, and publication state. Valid final states include `SUCCESS`, `NO_RELEASE`, and `FAILED`.

Local execution never claims that GitHub was updated. Only the authenticated workflow performs the final commit/push step.

## Limits

Gemini discovery is generative reasoning from the model's knowledge; the factory does not claim it performed live market research or user interviews. Generated products are deliberately restricted to small local-first browser apps so they can be validated safely and automatically. Duplicate detection is heuristic, so it reduces repeats but cannot mathematically guarantee every future idea is globally unique.
