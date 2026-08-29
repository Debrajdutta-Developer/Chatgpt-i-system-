"""Factory-owned validation for systems and legacy project profiles.

Generated source never chooses commands. The validator selects a fixed build/test
sequence from trusted project metadata and classifies every failure truthfully.
Generated executables run with a scrubbed environment so repository secrets are not
inherited by untrusted generated code.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
from pathlib import Path

from .models import ValidationResult

TRUSTED_EXECUTABLES = {"python", "node", "npm", "docker", "javac", "java", "cc"}
SYSTEM_PROFILES = {"systems-java", "systems-c", "systems-python"}
SAFE_ENV_KEYS = {
    "PATH", "HOME", "TMPDIR", "TMP", "TEMP", "LANG", "LC_ALL", "TZ",
    "JAVA_HOME", "JAVA_HOME_21_X64", "PYTHONHOME", "PYTHONPATH",
}


def classify(stderr: str, exit_code: int, kind: str) -> str | None:
    if exit_code == 0:
        return None
    text = stderr.lower()
    if any(token in text for token in (
        "no module named", "command not found", "not recognized as an internal",
        "could not find a version", "npm err! code e404",
    )):
        return "DEPENDENCY_FAILURE"
    if any(token in text for token in ("temporary failure", "connection reset", "etimedout", "econnreset")):
        return "NETWORK_FAILURE"
    return "TEST_FAILURE" if kind == "test" else "BUILD_FAILURE"


def _validation_env() -> dict[str, str]:
    """Return a minimal environment with no repository/API secrets."""
    env = {key: value for key, value in os.environ.items() if key in SAFE_ENV_KEYS and value}
    env.setdefault("PATH", os.defpath)
    env.setdefault("LANG", "C.UTF-8")
    return env


def _run(project: Path, command: list[str], kind: str, timeout: int = 180) -> ValidationResult:
    executable = command[0]
    if executable not in TRUSTED_EXECUTABLES and not executable.startswith("./build/"):
        raise ValueError(f"validation executable is not allowlisted: {executable}")
    try:
        completed = subprocess.run(
            command,
            cwd=project,
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
            env=_validation_env(),
        )
        output = (completed.stdout + completed.stderr).strip()
        return ValidationResult(
            command,
            completed.returncode,
            "PASS" if completed.returncode == 0 else "FAIL",
            classify(output, completed.returncode, kind),
            output[-7000:],
        )
    except FileNotFoundError as exc:
        return ValidationResult(command, 1, "FAIL", "DEPENDENCY_FAILURE", str(exc))
    except subprocess.TimeoutExpired as exc:
        return ValidationResult(command, 1, "FAIL", "TEST_FAILURE" if kind == "test" else "BUILD_FAILURE", f"command timed out after {exc.timeout}s")


def _static_result(errors: list[str], success: str) -> ValidationResult:
    return ValidationResult(
        ["factory", "static-product-check"],
        1 if errors else 0,
        "FAIL" if errors else "PASS",
        "BUILD_FAILURE" if errors else None,
        "\n".join(errors) if errors else success,
    )


def _metadata(project: Path) -> dict:
    path = project / "project.json"
    if not path.is_file():
        return {}
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else {}
    except (OSError, json.JSONDecodeError):
        return {}


def _assertion_signals(text: str) -> int:
    """Count explicit assertion/check invocations without depending on one helper name."""
    lower = text.lower()
    signals = 0
    # Java/JUnit-style or custom assert helpers, e.g. assertTrue(), assertEq().
    signals += len(re.findall(r"\bassert\w*\s*\(", lower))
    # Dependency-free suites often use checkEq(), verifyState(), expectValue(), etc.
    signals += len(re.findall(r"\b(?:check|verify|expect|require)\w*\s*\(", lower))
    # Python bare asserts and explicit AssertionError branches.
    signals += len(re.findall(r"(?m)^\s*assert\s+", lower))
    signals += lower.count("assertionerror")
    return signals


def _source_safety_errors(texts: dict[str, str]) -> list[str]:
    """Reject real shell/dynamic execution capabilities without substring false positives.

    Safety checks are applied to generated source, not README prose. Function-style
    checks use identifier boundaries, so harmless names such as `storage_system(` or
    `test_system(` are not mistaken for the C standard-library `system(...)` call.
    """
    errors: list[str] = []
    source_items = [(k, v) for k, v in texts.items() if k != "README.md"]
    source_only = "\n".join(v for _, v in source_items)
    lower = source_only.lower()

    for marker in ("todo", "coming soon", "lorem ipsum", "not implemented", "placeholder implementation"):
        if marker in lower:
            errors.append(f"unfinished/fake marker found: {marker}")

    literal_tokens = (
        "runtime.getruntime().exec", "processbuilder(", "child_process", "subprocess.",
        "os.system(", "node:vm",
    )
    for token in literal_tokens:
        if token in lower:
            errors.append(f"forbidden shell/dynamic execution capability found: {token}")

    for function_name in ("system", "popen", "eval", "exec"):
        pattern = rf"(?<![a-z0-9_]){function_name}\s*\("
        if re.search(pattern, lower):
            errors.append(f"forbidden shell/dynamic execution capability found: {function_name}(")

    for url in re.findall(r"https?://[^\s\"'`)>]+", source_only, flags=re.IGNORECASE):
        lu = url.lower()
        if "127.0.0.1" not in lu and "localhost" not in lu:
            errors.append(f"public-network URL is forbidden in generated source: {url[:120]}")
            break
    return errors


def _systems_required(profile: str) -> list[str]:
    common = ["README.md", "project.json", ".gitignore", ".github/workflows/ci.yml"]
    if profile == "systems-java":
        return common + [
            "src/main/java/factory/Core.java", "src/main/java/factory/Main.java",
            "src/test/java/factory/CoreTest.java", "src/test/java/factory/IntegrationTest.java",
        ]
    if profile == "systems-c":
        return common + [
            "src/engine.h", "src/engine.c", "src/main.c",
            "tests/test_engine.c", "tests/test_integration.c",
        ]
    if profile == "systems-python":
        return common + [
            "src/engine.py", "src/cli.py", "tests/test_engine.py", "tests/test_integration.py",
        ]
    raise ValueError(profile)


def _validate_systems(project: Path, profile: str, metadata: dict) -> list[ValidationResult]:
    errors: list[str] = []
    required = _systems_required(profile)
    texts: dict[str, str] = {}
    for name in required:
        path = project / name
        if not path.is_file() or path.stat().st_size < 40:
            errors.append(f"missing or too-small required file: {name}")
        elif path.suffix in {".md", ".json", ".java", ".c", ".h", ".py", ".yml"} or path.name == ".gitignore":
            texts[name] = path.read_text(encoding="utf-8", errors="replace")

    features = metadata.get("major_features", [])
    if not isinstance(features, list) or len(features) < 8:
        errors.append("hard systems release requires at least 8 declared major features")
    if metadata.get("difficulty") != "HARD_OR_EXTREME":
        errors.append("systems project is missing HARD_OR_EXTREME difficulty metadata")
    if metadata.get("factory_profile") != profile:
        errors.append("project profile metadata does not match selected validator profile")

    readme = texts.get("README.md", "")
    if len(readme.strip()) < 2200:
        errors.append("README.md is too small for an engineering-grade architecture/verification document")
    lower_readme = readme.lower()
    for section in ("architecture", "test", "limitation"):
        if section not in lower_readme:
            errors.append(f"README.md must document {section}")

    errors.extend(_source_safety_errors(texts))

    if profile == "systems-java" and not errors:
        core = texts["src/main/java/factory/Core.java"]
        main = texts["src/main/java/factory/Main.java"]
        unit = texts["src/test/java/factory/CoreTest.java"]
        integ = texts["src/test/java/factory/IntegrationTest.java"]
        for name, text in (("Core.java", core), ("Main.java", main), ("CoreTest.java", unit), ("IntegrationTest.java", integ)):
            if "package factory;" not in text:
                errors.append(f"{name} must use package factory")
        if len(core.strip()) < 2600:
            errors.append("Core.java is too small for a substantial systems engine")
        if len(main.strip()) < 900:
            errors.append("Main.java is too small for a useful runnable entrypoint")
        if "public static void main" not in main:
            errors.append("Main.java must expose a runnable main entrypoint")
        if "public static void main" not in unit or "public static void main" not in integ:
            errors.append("Java unit and integration suites must be executable main classes")
        if _assertion_signals(unit) < 8:
            errors.append("CoreTest.java must contain at least 8 meaningful checks")
        if _assertion_signals(integ) < 8:
            errors.append("IntegrationTest.java must contain at least 8 meaningful checks")

    if profile == "systems-c" and not errors:
        header = texts["src/engine.h"]
        engine = texts["src/engine.c"]
        main = texts["src/main.c"]
        unit = texts["tests/test_engine.c"]
        integ = texts["tests/test_integration.c"]
        if len(engine.strip()) < 3000:
            errors.append("engine.c is too small for a substantial systems engine")
        if len(header.strip()) < 500:
            errors.append("engine.h is too small to define a meaningful public API")
        if len(main.strip()) < 700 or "main(" not in main:
            errors.append("main.c must provide a useful runnable CLI")
        if "main(" not in unit or "main(" not in integ:
            errors.append("C unit and integration tests must each provide main()")
        if _assertion_signals(unit) < 8:
            errors.append("test_engine.c must contain at least 8 meaningful checks")
        if _assertion_signals(integ) < 8:
            errors.append("test_integration.c must contain at least 8 meaningful checks")

    if profile == "systems-python" and not errors:
        engine = texts["src/engine.py"]
        cli = texts["src/cli.py"]
        unit = texts["tests/test_engine.py"]
        integ = texts["tests/test_integration.py"]
        if len(engine.strip()) < 3000:
            errors.append("engine.py is too small for a substantial systems engine")
        if len(cli.strip()) < 800 or "argparse" not in cli:
            errors.append("cli.py must provide a substantial argparse-based runnable CLI")
        if "unittest" not in unit or "unittest" not in integ:
            errors.append("Python unit and integration suites must use unittest")
        if _assertion_signals(unit) < 8:
            errors.append("test_engine.py must contain at least 8 meaningful assertions")
        if _assertion_signals(integ) < 8:
            errors.append("test_integration.py must contain at least 8 meaningful assertions")
        for token in ("import requests", "urllib.request", "import socket"):
            if token in (engine + "\n" + cli).lower():
                errors.append(f"Python systems engine must remain local-only; forbidden network capability: {token}")

    results = [_static_result(errors, f"{profile} architecture, depth, safety and documentation gates passed")]
    if errors:
        return results

    build = project / "build"
    shutil.rmtree(build, ignore_errors=True)
    build.mkdir(parents=True, exist_ok=True)

    if profile == "systems-python":
        results.append(_run(project, ["python", "-m", "compileall", "-q", "src", "tests"], "build"))
        results.append(_run(project, ["python", "-m", "unittest", "discover", "-s", "tests", "-p", "test_engine.py", "-v"], "test"))
        results.append(_run(project, ["python", "-m", "unittest", "discover", "-s", "tests", "-p", "test_integration.py", "-v"], "test"))
        return results

    if profile == "systems-java":
        (build / "classes").mkdir(parents=True, exist_ok=True)
        sources = sorted(str(p.relative_to(project)) for p in (project / "src").rglob("*.java"))
        results.append(_run(project, ["javac", "-Xlint:all", "-Werror", "-d", "build/classes", *sources], "build", timeout=240))
        if results[-1].exit_code == 0:
            results.append(_run(project, ["java", "-ea", "-cp", "build/classes", "factory.CoreTest"], "test", timeout=180))
            results.append(_run(project, ["java", "-ea", "-cp", "build/classes", "factory.IntegrationTest"], "test", timeout=180))
        return results

    common = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", "src", "src/engine.c"]
    results.append(_run(project, [*common, "tests/test_engine.c", "-o", "build/test_engine"], "build", timeout=240))
    if results[-1].exit_code == 0:
        results.append(_run(project, ["./build/test_engine"], "test", timeout=120))
    results.append(_run(project, [*common, "tests/test_integration.c", "-o", "build/test_integration"], "build", timeout=240))
    if results[-1].exit_code == 0:
        results.append(_run(project, ["./build/test_integration"], "test", timeout=120))
    results.append(_run(project, [*common, "src/main.c", "-o", "build/app"], "build", timeout=240))
    return results


def _validate_fullstack(project: Path) -> list[ValidationResult]:
    """Compatibility validator for already-released production-style Node projects."""
    required = [
        "README.md", "package.json", "src/App.tsx", "server/domain.mjs", "server/db.mjs",
        "server/server.mjs", "tests/domain.test.mjs", "tests/api.test.mjs", "Dockerfile", "project.json",
    ]
    errors = [f"missing required legacy full-stack file: {name}" for name in required if not (project / name).is_file()]
    results = [_static_result(errors, "legacy full-stack file contract passed")]
    if errors:
        return results
    if not shutil.which("npm"):
        return results + [ValidationResult(["npm", "test"], 1, "FAIL", "DEPENDENCY_FAILURE", "npm unavailable")]
    install = _run(project, ["npm", "install", "--ignore-scripts", "--no-audit", "--no-fund"], "build", timeout=300)
    results.append(install)
    if install.exit_code == 0:
        results.append(_run(project, ["npm", "test"], "test", timeout=180))
        results.append(_run(project, ["npm", "run", "build"], "build", timeout=240))
    shutil.rmtree(project / "node_modules", ignore_errors=True)
    shutil.rmtree(project / "dist", ignore_errors=True)
    return results


def _validate_legacy_web(project: Path) -> list[ValidationResult]:
    errors: list[str] = []
    required = ["README.md", "index.html", "style.css", "core.js", "app.js", "test-core.js", "project.json"]
    for name in required:
        path = project / name
        if not path.is_file() or path.stat().st_size < 80:
            errors.append(f"missing or too-small required file: {name}")
    results = [_static_result(errors, "legacy browser file contract passed")]
    if errors:
        return results
    if not shutil.which("node"):
        return results + [ValidationResult(["node", "test-core.js"], 1, "FAIL", "DEPENDENCY_FAILURE", "node unavailable")]
    for name in ("core.js", "app.js", "test-core.js"):
        results.append(_run(project, ["node", "--check", name], "build"))
    results.append(_run(project, ["node", "test-core.js"], "test"))
    return results


def validate(project: Path) -> list[ValidationResult]:
    metadata = _metadata(project)
    profile = metadata.get("factory_profile")
    if profile in SYSTEM_PROFILES:
        return _validate_systems(project, profile, metadata)
    if (project / "package.json").is_file() and (project / "src" / "App.tsx").is_file():
        return _validate_fullstack(project)
    if (project / "index.html").is_file() and (project / "test-core.js").is_file():
        return _validate_legacy_web(project)
    commands = [
        (["python", "-m", "unittest", "discover", "-s", "tests", "-v"], "test"),
        (["python", "-m", "compileall", "-q", "src", "tests"], "build"),
    ]
    return [_run(project, command, kind) for command, kind in commands]
