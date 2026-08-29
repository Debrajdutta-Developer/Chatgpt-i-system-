"""Allowlisted project validation with truthful failure classification."""
from __future__ import annotations
import subprocess
from pathlib import Path
from .models import ValidationResult

ALLOWED = {("python", "-m", "unittest"), ("python", "-m", "compileall")}

def classify(stderr: str, exit_code: int, kind: str) -> str | None:
    if exit_code == 0: return None
    text = stderr.lower()
    if "no module named" in text or "could not find a version" in text: return "DEPENDENCY_FAILURE"
    if "temporary failure" in text or "connection" in text: return "NETWORK_FAILURE"
    return "TEST_FAILURE" if kind == "test" else "BUILD_FAILURE"

def validate(project: Path) -> list[ValidationResult]:
    commands = [(["python", "-m", "unittest", "discover", "-s", "tests", "-v"], "test"), (["python", "-m", "compileall", "-q", "src", "tests"], "build")]
    results = []
    for command, kind in commands:
        if tuple(command[:3]) not in ALLOWED: raise ValueError("validation command is not allowlisted")
        completed = subprocess.run(command, cwd=project, text=True, capture_output=True, timeout=120, check=False)
        output = (completed.stdout + completed.stderr).strip()
        results.append(ValidationResult(command, completed.returncode, "PASS" if completed.returncode == 0 else "FAIL", classify(output, completed.returncode, kind), output[-4000:]))
    return results
