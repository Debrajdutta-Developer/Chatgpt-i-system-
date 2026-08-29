"""Allowlisted project validation with truthful failure classification."""
from __future__ import annotations
import shutil
import subprocess
from pathlib import Path

from .models import ValidationResult

ALLOWED = {
    ("python", "-m", "unittest"),
    ("python", "-m", "compileall"),
    ("node", "--check"),
    ("node", "test-core.js"),
}


def classify(stderr: str, exit_code: int, kind: str) -> str | None:
    if exit_code == 0:
        return None
    text = stderr.lower()
    if "no module named" in text or "could not find a version" in text:
        return "DEPENDENCY_FAILURE"
    if "temporary failure" in text or "connection" in text:
        return "NETWORK_FAILURE"
    return "TEST_FAILURE" if kind == "test" else "BUILD_FAILURE"


def _run(project: Path, command: list[str], kind: str) -> ValidationResult:
    prefix = tuple(command[:3]) if command[0] == "python" else tuple(command[:2])
    if prefix not in ALLOWED:
        raise ValueError("validation command is not allowlisted")
    completed = subprocess.run(command, cwd=project, text=True, capture_output=True, timeout=120, check=False)
    output = (completed.stdout + completed.stderr).strip()
    return ValidationResult(
        command,
        completed.returncode,
        "PASS" if completed.returncode == 0 else "FAIL",
        classify(output, completed.returncode, kind),
        output[-4000:],
    )


def _validate_web(project: Path) -> list[ValidationResult]:
    errors: list[str] = []
    required = ["README.md", "index.html", "style.css", "core.js", "app.js", "test-core.js", "project.json"]
    for name in required:
        path = project / name
        if not path.is_file() or path.stat().st_size < 80:
            errors.append(f"missing or too-small required file: {name}")

    if not errors:
        html = (project / "index.html").read_text(encoding="utf-8", errors="replace")
        js = (project / "app.js").read_text(encoding="utf-8", errors="replace")
        core = (project / "core.js").read_text(encoding="utf-8", errors="replace")
        tests = (project / "test-core.js").read_text(encoding="utf-8", errors="replace")
        css = (project / "style.css").read_text(encoding="utf-8", errors="replace")
        readme = (project / "README.md").read_text(encoding="utf-8", errors="replace")
        lower_html = html.lower()
        lower_all = "\n".join([html, js, core, tests, css, readme]).lower()

        if "<html" not in lower_html or "</html>" not in lower_html:
            errors.append("index.html is not a complete HTML document")
        if "style.css" not in html or "app.js" not in html or "core.js" not in html:
            errors.append("index.html must reference style.css, core.js and app.js")
        elif html.find("core.js") > html.find("app.js"):
            errors.append("index.html must load core.js before app.js")
        if "globalthis.productcore" not in core.lower():
            errors.append("core.js must expose the real engine as globalThis.ProductCore")
        if "productcore" not in js.lower():
            errors.append("app.js must consume ProductCore instead of implementing a disconnected UI demo")
        if "core.js" not in tests.lower():
            errors.append("test-core.js must execute the real core.js engine")
        assertion_signals = sum(tests.lower().count(token) for token in ("assert", "throw new error", "expect("))
        if assertion_signals < 5:
            errors.append("test-core.js must contain at least 5 meaningful assertions/checks")
        if len(css.strip()) < 1000:
            errors.append("style.css is too small for a portfolio-grade interface")
        if len(js.strip()) < 1200:
            errors.append("app.js is too small for a substantial product workflow")
        if len(core.strip()) < 1000:
            errors.append("core.js is too small for a substantial deterministic product engine")
        if len(readme.strip()) < 1200:
            errors.append("README.md is too small for production-style documentation")
        for marker in ("todo", "coming soon", "lorem ipsum"):
            if marker in lower_all:
                errors.append(f"placeholder marker found: {marker}")
        for forbidden in ("eval(", "new function(", "document.write(", "websocket(", "fetch("):
            if forbidden in lower_all:
                errors.append(f"forbidden remote/dynamic behavior found: {forbidden}")
        remote_patterns = ("<script src=\"http", "<script src='http", "<link href=\"http", "<link href='http")
        for forbidden in remote_patterns:
            if forbidden in lower_all:
                errors.append(f"forbidden remote asset found: {forbidden}")

    static = ValidationResult(
        ["factory", "static-web-check"],
        1 if errors else 0,
        "FAIL" if errors else "PASS",
        "BUILD_FAILURE" if errors else None,
        "\n".join(errors) if errors else "architecture, completeness, local-only policy, and document structure passed",
    )
    results = [static]

    node = shutil.which("node")
    if node:
        for name in ("core.js", "app.js", "test-core.js"):
            if (project / name).is_file():
                results.append(_run(project, ["node", "--check", name], "build"))
        if (project / "test-core.js").is_file():
            results.append(_run(project, ["node", "test-core.js"], "test"))
    else:
        results.append(ValidationResult(
            ["node", "test-core.js"], 1, "FAIL", "DEPENDENCY_FAILURE", "node executable is unavailable"
        ))
    return results


def validate(project: Path) -> list[ValidationResult]:
    if (project / "index.html").is_file():
        return _validate_web(project)
    commands = [
        (["python", "-m", "unittest", "discover", "-s", "tests", "-v"], "test"),
        (["python", "-m", "compileall", "-q", "src", "tests"], "build"),
    ]
    return [_run(project, command, kind) for command, kind in commands]
