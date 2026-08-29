"""Evidence-based quality gate; critical validation always overrides score."""
from __future__ import annotations

from pathlib import Path

from .models import Idea, ValidationResult


def _command_starts(result: ValidationResult, prefix: list[str]) -> bool:
    return result.command[: len(prefix)] == prefix


def evaluate(project: Path, idea: Idea, results: list[ValidationResult], threshold: int) -> tuple[dict[str, int], int, bool]:
    critical = [r for r in results if r.critical]
    all_pass = bool(critical) and all(r.exit_code == 0 for r in critical)
    readme = project / "README.md"
    is_fullstack = (project / "package.json").is_file() and (project / "src" / "App.tsx").is_file()
    is_legacy_web = (project / "index.html").is_file() and not is_fullstack
    has_python_product = (project / "src").is_dir() and not is_fullstack

    if is_fullstack:
        functional_test_pass = any(_command_starts(r, ["npm", "test"]) and r.status == "PASS" for r in results)
        production_build_pass = any(_command_starts(r, ["npm", "run", "build"]) and r.status == "PASS" for r in results)
        container_build_pass = any(_command_starts(r, ["docker", "build"]) and r.status == "PASS" for r in results)
        architecture_ok = all((project / name).is_file() for name in (
            "src/App.tsx", "server/domain.mjs", "server/db.mjs", "server/server.mjs",
            "tests/domain.test.mjs", "tests/api.test.mjs", "Dockerfile", ".github/workflows/ci.yml",
        ))
        feature_depth = len(getattr(idea, "major_features", ()) or ()) >= 6
        documentation_ok = readme.is_file() and readme.stat().st_size >= 2200
        sophistication_ok = architecture_ok and feature_depth and production_build_pass and container_build_pass
        scores = {
            "real_problem": 10 if idea.problem and idea.target_user and idea.solution else 0,
            "architecture": 15 if architecture_ok else 0,
            "correctness": 20 if all_pass else 0,
            "functional_testing": 20 if functional_test_pass else 0,
            "production_build": 10 if production_build_pass else 0,
            "containerization": 5 if container_build_pass else 0,
            "documentation": 10 if documentation_ok else 0,
            "security": 5 if not any((project / name).exists() for name in (".env", "credentials.json", "secrets.json")) else 0,
            "feature_depth": 5 if feature_depth else 0,
        }
        total = sum(scores.values())
        release = (
            all_pass
            and functional_test_pass
            and production_build_pass
            and container_build_pass
            and sophistication_ok
            and documentation_ok
            and total >= threshold
        )
        return scores, total, release

    if is_legacy_web:
        functional_test_pass = any(_command_starts(r, ["node", "test-core.js"]) and r.status == "PASS" for r in results)
        architecture_ok = all((project / name).is_file() for name in ("core.js", "app.js", "test-core.js"))
        feature_depth = len(getattr(idea, "major_features", ()) or ()) >= 5
        scores = {
            "usefulness": 15 if idea.problem and idea.target_user and idea.solution else 0,
            "completeness": 15 if readme.is_file() else 0,
            "correctness": 20 if all_pass else 0,
            "functional_testing": 20 if functional_test_pass else 0,
            "documentation": 10 if readme.is_file() and readme.stat().st_size >= 1200 else 0,
            "security": 10 if not any((project / name).exists() for name in (".env", "credentials.json", "secrets.json")) else 0,
            "sophistication": 10 if architecture_ok and feature_depth else 0,
        }
        total = sum(scores.values())
        return scores, total, all_pass and functional_test_pass and architecture_ok and feature_depth and total >= threshold

    functional_test_pass = any(r.status == "PASS" and "unittest" in r.command for r in results)
    architecture_ok = has_python_product and (project / "tests").is_dir()
    feature_depth = len(getattr(idea, "major_features", ()) or ()) >= 3
    scores = {
        "usefulness": 20 if idea.problem and idea.target_user and idea.solution else 0,
        "completeness": 20 if has_python_product and readme.is_file() else 0,
        "correctness": 20 if all_pass else 0,
        "functional_testing": 20 if functional_test_pass else 0,
        "documentation": 10 if readme.is_file() and readme.stat().st_size >= 500 else 0,
        "security": 5 if not any((project / name).exists() for name in (".env", "credentials.json", "secrets.json")) else 0,
        "feature_depth": 5 if feature_depth else 0,
    }
    total = sum(scores.values())
    return scores, total, all_pass and functional_test_pass and architecture_ok and total >= min(threshold, 90)
