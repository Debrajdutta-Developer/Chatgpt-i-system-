"""Evidence-based quality gate; critical validation always overrides score."""
from __future__ import annotations

import json
from pathlib import Path

from .models import Idea, ValidationResult


def _command_starts(result: ValidationResult, prefix: list[str]) -> bool:
    return result.command[: len(prefix)] == prefix


def _metadata(project: Path) -> dict:
    path = project / "project.json"
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else {}
    except (OSError, json.JSONDecodeError):
        return {}


def _systems_architecture_ok(project: Path, profile: str) -> bool:
    if profile == "systems-java":
        paths = (
            "src/main/java/factory/Core.java", "src/main/java/factory/Main.java",
            "src/test/java/factory/CoreTest.java", "src/test/java/factory/IntegrationTest.java",
        )
    elif profile == "systems-c":
        paths = ("src/engine.h", "src/engine.c", "src/main.c", "tests/test_engine.c", "tests/test_integration.c")
    elif profile == "systems-python":
        paths = ("src/engine.py", "src/cli.py", "tests/test_engine.py", "tests/test_integration.py")
    else:
        return False
    return all((project / path).is_file() for path in (*paths, ".github/workflows/ci.yml", "README.md"))


def evaluate(project: Path, idea: Idea, results: list[ValidationResult], threshold: int) -> tuple[dict[str, int], int, bool]:
    critical = [r for r in results if r.critical]
    all_pass = bool(critical) and all(r.exit_code == 0 for r in critical)
    readme = project / "README.md"
    metadata = _metadata(project)
    profile = metadata.get("factory_profile")

    if profile in {"systems-java", "systems-c", "systems-python"}:
        architecture_ok = _systems_architecture_ok(project, profile)
        feature_depth = len(getattr(idea, "major_features", ()) or ()) >= 8
        documentation_ok = readme.is_file() and readme.stat().st_size >= 2200

        if profile == "systems-java":
            build_pass = any(_command_starts(r, ["javac"]) and r.status == "PASS" for r in results)
            unit_pass = any("factory.CoreTest" in r.command and r.status == "PASS" for r in results)
            integration_pass = any("factory.IntegrationTest" in r.command and r.status == "PASS" for r in results)
        elif profile == "systems-c":
            build_pass = any(r.command and r.command[0] == "cc" and "src/main.c" in r.command and r.status == "PASS" for r in results)
            unit_pass = any(r.command[:1] == ["./build/test_engine"] and r.status == "PASS" for r in results)
            integration_pass = any(r.command[:1] == ["./build/test_integration"] and r.status == "PASS" for r in results)
        else:
            build_pass = any(_command_starts(r, ["python", "-m", "compileall"]) and r.status == "PASS" for r in results)
            unit_pass = any("test_engine.py" in r.command and r.status == "PASS" for r in results)
            integration_pass = any("test_integration.py" in r.command and r.status == "PASS" for r in results)

        scores = {
            "real_problem": 10 if idea.problem and idea.target_user and idea.solution else 0,
            "systems_architecture": 15 if architecture_ok else 0,
            "correctness": 25 if all_pass else 0,
            "unit_testing": 12 if unit_pass else 0,
            "integration_testing": 13 if integration_pass else 0,
            "reproducible_build": 10 if build_pass else 0,
            "documentation": 10 if documentation_ok else 0,
            "feature_depth": 5 if feature_depth else 0,
        }
        total = sum(scores.values())
        release = (
            all_pass
            and architecture_ok
            and feature_depth
            and documentation_ok
            and build_pass
            and unit_pass
            and integration_pass
            and total >= threshold
        )
        return scores, total, release

    is_fullstack = (project / "package.json").is_file() and (project / "src" / "App.tsx").is_file()
    is_legacy_web = (project / "index.html").is_file() and not is_fullstack
    has_python_product = (project / "src").is_dir() and not is_fullstack

    if is_fullstack:
        functional_test_pass = any(_command_starts(r, ["npm", "test"]) and r.status == "PASS" for r in results)
        production_build_pass = any(_command_starts(r, ["npm", "run", "build"]) and r.status == "PASS" for r in results)
        architecture_ok = all((project / name).is_file() for name in (
            "src/App.tsx", "server/domain.mjs", "server/db.mjs", "server/server.mjs",
            "tests/domain.test.mjs", "tests/api.test.mjs",
        ))
        feature_depth = len(getattr(idea, "major_features", ()) or ()) >= 6
        documentation_ok = readme.is_file() and readme.stat().st_size >= 2200
        scores = {
            "real_problem": 10 if idea.problem and idea.target_user and idea.solution else 0,
            "architecture": 15 if architecture_ok else 0,
            "correctness": 25 if all_pass else 0,
            "functional_testing": 20 if functional_test_pass else 0,
            "production_build": 10 if production_build_pass else 0,
            "documentation": 10 if documentation_ok else 0,
            "feature_depth": 10 if feature_depth else 0,
        }
        total = sum(scores.values())
        return scores, total, all_pass and functional_test_pass and production_build_pass and architecture_ok and documentation_ok and total >= threshold

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
        return scores, total, all_pass and functional_test_pass and architecture_ok and feature_depth and total >= min(threshold, 90)

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
