"""Command-line interface for Env Sentinel."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from .core import audit


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="env-sentinel",
        description="Check that environment variables used, documented, and configured agree.",
    )
    p.add_argument("--root", type=Path, default=Path.cwd(), help="source tree to scan (default: current directory)")
    p.add_argument("--contract", type=Path, default=Path(".env.example"), help="documented dotenv contract")
    p.add_argument("--env", dest="environment", type=Path, help="optional local dotenv file to compare")
    p.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    return p


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    root = args.root.resolve()
    contract = args.contract if args.contract.is_absolute() else root / args.contract
    environment = args.environment
    if environment is not None and not environment.is_absolute():
        environment = root / environment
    if not root.is_dir():
        print(f"error: source root does not exist: {root}", file=sys.stderr)
        return 2
    if not contract.is_file():
        print(f"error: contract file does not exist: {contract}", file=sys.stderr)
        return 2
    if environment is not None and not environment.is_file():
        print(f"error: environment file does not exist: {environment}", file=sys.stderr)
        return 2

    report = audit(root, contract, environment)
    if args.json:
        print(json.dumps(report.to_dict(), indent=2))
    elif report.findings:
        print(f"Env Sentinel found {len(report.findings)} issue(s):")
        for finding in report.findings:
            label = f" {finding.key}" if finding.key else ""
            print(f"- [{finding.kind}]{label}: {finding.detail}")
    else:
        print(f"Env Sentinel: OK ({len(report.documented)} variables checked)")
    return 1 if report.findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
