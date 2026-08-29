# Env Sentinel

Environment variables form an implicit contract between source code, deployment
configuration, and developers. That contract commonly drifts: a renamed variable
is still in `.env.example`, a required key is missing locally, or a new secret is
used without documentation. These mistakes often surface only during deployment.

Env Sentinel is a dependency-free, local-first CLI that compares those three
views without reading or printing secret values. It scans common source languages
for statically named environment-variable access, parses a dotenv contract, and
optionally checks a real dotenv file.

## Features

- Detects variables used in code but absent from `.env.example`.
- Detects documented variables no longer referenced in code.
- Optionally detects missing and unexpected keys in a local `.env` file.
- Reports invalid and duplicate dotenv declarations.
- Supports Python, JavaScript/TypeScript, Ruby, Java, Rust, and common source files.
- Ignores dependencies, VCS metadata, build output, and secret values.
- Provides human-readable and JSON output suitable for CI.

Dynamic variable names (for example, `os.getenv(prefix + name)`) cannot be
discovered statically and should be documented separately.

## Install and run

Python 3.10 or newer is required. No runtime dependencies are needed.

```bash
python -m pip install .
env-sentinel --root . --contract .env.example
env-sentinel --root . --contract .env.example --env .env
env-sentinel --root . --json
```

Paths passed to `--contract` and `--env` are resolved relative to `--root`.
Exit code `0` means the contract is consistent, `1` means findings were detected,
and `2` means the command could not run because an input was invalid.

Example finding:

```text
Env Sentinel found 1 issue(s):
- [undocumented] DATABASE_URL: referenced in source but absent from contract
```

JSON output contains the input paths, sorted documented/configured/referenced key
lists, and structured findings. It never contains dotenv values.

## Development

```bash
python -m unittest discover -s tests -v
python -m compileall -q env_sentinel tests
```

## Architecture

`env_sentinel/core.py` owns dotenv parsing, source scanning, and comparison.
`env_sentinel/cli.py` validates inputs, formats output, and defines exit codes.
The standard library keeps installation fast and makes offline use practical.

