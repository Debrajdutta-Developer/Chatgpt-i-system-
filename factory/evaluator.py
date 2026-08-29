"""Evidence-based quality gate; critical validation always overrides score."""
from __future__ import annotations
from pathlib import Path
from .models import Idea, ValidationResult

def evaluate(project: Path, idea: Idea, results: list[ValidationResult], threshold: int) -> tuple[dict[str, int], int, bool]:
    all_pass = bool(results) and all(r.exit_code == 0 for r in results if r.critical)
    scores = {
      "usefulness": 20 if idea.problem and idea.target_user else 0,
      "completeness": 20 if (project / "src").is_dir() and (project / "README.md").is_file() else 0,
      "correctness": 20 if all_pass else 0,
      "testing": 15 if any(r.status == "PASS" and "unittest" in r.command for r in results) else 0,
      "documentation": 10 if (project / "README.md").stat().st_size >= 500 else 0,
      "security": 10 if not any((project / name).exists() for name in (".env", "credentials.json")) else 0,
      "novelty": 5,
    }
    total = sum(scores.values())
    return scores, total, all_pass and total >= threshold
