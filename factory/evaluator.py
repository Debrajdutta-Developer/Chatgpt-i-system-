"""Evidence-based quality gate; critical validation always overrides score."""
from __future__ import annotations
from pathlib import Path
from .models import Idea, ValidationResult


def evaluate(project: Path, idea: Idea, results: list[ValidationResult], threshold: int) -> tuple[dict[str, int], int, bool]:
    critical = [r for r in results if r.critical]
    all_pass = bool(critical) and all(r.exit_code == 0 for r in critical)
    has_product = (project / "src").is_dir() or (project / "index.html").is_file()
    validated = bool(results) and all(r.status == "PASS" for r in results if r.critical)
    readme = project / "README.md"
    scores = {
      "usefulness": 20 if idea.problem and idea.target_user and idea.solution else 0,
      "completeness": 20 if has_product and readme.is_file() else 0,
      "correctness": 20 if all_pass else 0,
      "testing": 15 if validated else 0,
      "documentation": 10 if readme.is_file() and readme.stat().st_size >= 500 else 0,
      "security": 10 if not any((project / name).exists() for name in (".env", "credentials.json", "secrets.json")) else 0,
      "novelty": 5,
    }
    total = sum(scores.values())
    return scores, total, all_pass and total >= threshold
