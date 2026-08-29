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
    required = ["README.md", "index.html", "style.css", "app.js", "project.json"]
    for name in required:
        path = project / name
        if not path.is_file() or path.stat().st_size < 40:
            errors.append(f"missing or too-small required file: {name}")

    if not errors:
        html = (project / "index.html").read_text(encoding="utf-8", errors="replace")
        js = (project / "app.js").read_text(encoding="utf-8", errors="replace")
        css = (project / "style.css").read_text(encoding="utf-8", errors="replace")
        readme = (project / "README.md").read_text(encoding="utf-8", errors="replace")
        lower_html = html.lower()
        lower_all = "\n".join([html, js, css, readme]).lower()
        if "<html" not in lower_html or "</html>" not in lower_html:
            errors.append("index.html is not a complete HTML document")
        if "style.css" not in html or "app.js" not in html:
            errors.append("index.html must reference style.css and app.js")
        if len(css.strip()) < 200:
            errors.append("style.css is suspiciously small")
        if len(js.strip()) < 200:
            errors.append("app.js is suspiciously small")
        for marker in ("todo", "coming soon", "lorem ipsum", "placeholder"):
            if marker in lower_all:
                errors.append(f"placeholder marker found: {marker}")
        for forbidden in ("eval(", "new function(", "document.write(", "websocket(", "fetch(", "http://", "https://"):
            if forbidden in lower_all:
                errors.append(f"forbidden remote/dynamic behavior found: {forbidden}")

    static = ValidationResult(
        ["factory", "static-web-check"],
        1 if errors else 0,
        "FAIL" if errors else "PASS",
        "BUILD_FAILURE" if errors else None,
        "\n".join(errors) if errors else "required files, local-only policy, and document structure passed",
    )
    results = [static]

    node = shutil.which("node")
    if node and (project / "app.js").is_file():
        results.append(_run(project, ["node", "--check", "app.js"], "build"))
    else:
        results.append(ValidationResult(
            ["node", "--check", "app.js"], 1, "FAIL", "DEPENDENCY_FAILURE", "node executable is unavailable"
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
