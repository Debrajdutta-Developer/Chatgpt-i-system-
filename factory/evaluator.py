"""Evidence-based quality gate; critical validation always overrides score."""
from __future__ import annotations
from pathlib import Path
from .models import Idea, ValidationResult


def evaluate(project: Path, idea: Idea, results: list[ValidationResult], threshold: int) -> tuple[dict[str, int], int, bool]:
    critical = [r for r in results if r.critical]
    all_pass = bool(critical) and all(r.exit_code == 0 for r in critical)
    has_product = (project / "src").is_dir() or (project / "index.html").is_file()
    readme = project / "README.md"
    is_web = (project / "index.html").is_file()
    functional_test_pass = any(
        r.command[:2] == ["node", "test-core.js"] and r.status == "PASS"
        for r in results
    ) if is_web else any(r.status == "PASS" and "unittest" in r.command for r in results)
    architecture_ok = (
        all((project / name).is_file() for name in ("core.js", "app.js", "test-core.js"))
        if is_web else (project / "src").is_dir() and (project / "tests").is_dir()
    )
    feature_depth = len(getattr(idea, "major_features", ()) or ()) >= (5 if is_web else 3)
    scores = {
        "usefulness": 15 if idea.problem and idea.target_user and idea.solution else 0,
        "completeness": 15 if has_product and readme.is_file() else 0,
        "correctness": 20 if all_pass else 0,
        "functional_testing": 20 if functional_test_pass else 0,
        "documentation": 10 if readme.is_file() and readme.stat().st_size >= (1200 if is_web else 500) else 0,
        "security": 10 if not any((project / name).exists() for name in (".env", "credentials.json", "secrets.json")) else 0,
        "sophistication": 10 if architecture_ok and feature_depth else 0,
    }
    total = sum(scores.values())
    release = all_pass and functional_test_pass and architecture_ok and feature_depth and total >= threshold
    return scores, total, release
