"""Scanning and comparison logic for Env Sentinel."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path
import re


KEY_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
REFERENCE_PATTERNS = (
    re.compile(r"\bprocess\.env\.([A-Za-z_][A-Za-z0-9_]*)"),
    re.compile(r"\bprocess\.env\[['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]\]"),
    re.compile(r"\b(?:os\.)?(?:getenv|environ\.get)\(\s*['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]"),
    re.compile(r"\bos\.environ\[['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]\]"),
    re.compile(r"\bENV\[['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]\]"),
    re.compile(r"\bSystem\.getenv\(\s*['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]"),
    re.compile(r"\bstd::env::var\(\s*['\"]([A-Za-z_][A-Za-z0-9_]*)['\"]"),
)
SOURCE_SUFFIXES = {
    ".c", ".cc", ".cpp", ".cs", ".go", ".java", ".js", ".jsx", ".mjs",
    ".php", ".py", ".rb", ".rs", ".sh", ".ts", ".tsx",
}
IGNORED_DIRS = {".git", ".hg", ".svn", ".venv", "venv", "node_modules", "dist", "build", "vendor"}


@dataclass(frozen=True)
class Finding:
    kind: str
    key: str
    detail: str


@dataclass(frozen=True)
class Report:
    contract_file: str
    environment_file: str | None
    documented: list[str]
    configured: list[str]
    referenced: list[str]
    findings: list[Finding]

    def to_dict(self) -> dict:
        return asdict(self)


def parse_dotenv(path: Path) -> tuple[set[str], list[Finding]]:
    """Return keys in a dotenv file and syntax/duplicate findings."""
    keys: set[str] = set()
    findings: list[Finding] = []
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[7:].lstrip()
        if "=" not in line:
            findings.append(Finding("invalid-line", "", f"{path}:{number}: expected KEY=VALUE"))
            continue
        key = line.split("=", 1)[0].strip()
        if not KEY_RE.fullmatch(key):
            findings.append(Finding("invalid-key", key, f"{path}:{number}: invalid variable name"))
        elif key in keys:
            findings.append(Finding("duplicate", key, f"{path}:{number}: duplicate declaration"))
        else:
            keys.add(key)
    return keys, findings


def scan_references(root: Path, excluded_files: set[Path] | None = None) -> set[str]:
    """Find statically named environment variables in common source languages."""
    excluded = {p.resolve() for p in (excluded_files or set())}
    references: set[str] = set()
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        if any(part in IGNORED_DIRS for part in path.relative_to(root).parts):
            continue
        if path.resolve() in excluded:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for pattern in REFERENCE_PATTERNS:
            references.update(pattern.findall(text))
    return references


def audit(root: Path, contract: Path, environment: Path | None = None) -> Report:
    """Compare the documented contract with configuration and source usage."""
    documented, findings = parse_dotenv(contract)
    configured: set[str] = set()
    if environment is not None:
        configured, env_findings = parse_dotenv(environment)
        findings.extend(env_findings)
    referenced = scan_references(root, {contract, environment} if environment else {contract})

    for key in sorted(referenced - documented):
        findings.append(Finding("undocumented", key, "referenced in source but absent from contract"))
    for key in sorted(documented - referenced):
        findings.append(Finding("unused", key, "documented in contract but not statically referenced"))
    if environment is not None:
        for key in sorted(documented - configured):
            findings.append(Finding("missing", key, "documented but absent from environment file"))
        for key in sorted(configured - documented):
            findings.append(Finding("unexpected", key, "configured but absent from contract"))

    return Report(
        str(contract), str(environment) if environment else None,
        sorted(documented), sorted(configured), sorted(referenced), findings,
    )
