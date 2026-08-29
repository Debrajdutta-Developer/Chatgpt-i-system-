"""Allowlisted validation with truthful failure classification.

Generated products may contain untrusted source, but validation commands are fixed
by the factory and secrets are not passed to generated project processes.
"""
from __future__ import annotations

import json
import re
import shutil
import subprocess
from pathlib import Path

from .models import ValidationResult

ALLOWED = {
    ("python", "-m", "unittest"),
    ("python", "-m", "compileall"),
    ("node", "--check"),
    ("node", "test-core.js"),
    ("npm", "install"),
    ("npm", "test"),
    ("npm", "run"),
    ("docker", "build"),
}


def classify(stderr: str, exit_code: int, kind: str) -> str | None:
    if exit_code == 0:
        return None
    text = stderr.lower()
    if "no module named" in text or "could not find a version" in text or "npm err! code e404" in text:
        return "DEPENDENCY_FAILURE"
    if "temporary failure" in text or "connection" in text or "etimedout" in text or "econnreset" in text:
        return "NETWORK_FAILURE"
    return "TEST_FAILURE" if kind == "test" else "BUILD_FAILURE"


def _run(project: Path, command: list[str], kind: str, timeout: int = 120) -> ValidationResult:
    prefix = tuple(command[:3]) if command[0] == "python" else tuple(command[:2])
    if prefix not in ALLOWED:
        raise ValueError("validation command is not allowlisted")
    completed = subprocess.run(
        command,
        cwd=project,
        text=True,
        capture_output=True,
        timeout=timeout,
        check=False,
    )
    output = (completed.stdout + completed.stderr).strip()
    return ValidationResult(
        command,
        completed.returncode,
        "PASS" if completed.returncode == 0 else "FAIL",
        classify(output, completed.returncode, kind),
        output[-6000:],
    )


def _static_result(errors: list[str], success: str) -> ValidationResult:
    return ValidationResult(
        ["factory", "static-product-check"],
        1 if errors else 0,
        "FAIL" if errors else "PASS",
        "BUILD_FAILURE" if errors else None,
        "\n".join(errors) if errors else success,
    )


def _validate_fullstack(project: Path) -> list[ValidationResult]:
    errors: list[str] = []
    required = [
        "README.md", "package.json", "tsconfig.json", "vite.config.js", "index.html",
        "src/main.tsx", "src/App.tsx", "src/styles.css",
        "server/domain.mjs", "server/db.mjs", "server/server.mjs",
        "tests/domain.test.mjs", "tests/api.test.mjs", "Dockerfile",
        ".dockerignore", ".github/workflows/ci.yml", "project.json",
    ]
    for name in required:
        path = project / name
        if not path.is_file() or path.stat().st_size < 40:
            errors.append(f"missing or too-small required file: {name}")

    texts: dict[str, str] = {}
    if not errors:
        for name in required:
            texts[name] = (project / name).read_text(encoding="utf-8", errors="replace")
        try:
            package = json.loads(texts["package.json"])
        except json.JSONDecodeError:
            package = {}
            errors.append("package.json is invalid JSON")

        scripts = package.get("scripts", {}) if isinstance(package, dict) else {}
        for script in ("test", "build", "start"):
            if not isinstance(scripts, dict) or not scripts.get(script):
                errors.append(f"package.json missing trusted {script} script")
        deps = package.get("dependencies", {}) if isinstance(package, dict) else {}
        dev_deps = package.get("devDependencies", {}) if isinstance(package, dict) else {}
        if set(deps) != {"react", "react-dom"}:
            errors.append("runtime dependency set must be exactly react and react-dom")
        expected_dev = {"@types/react", "@types/react-dom", "@vitejs/plugin-react", "typescript", "vite"}
        if set(dev_deps) != expected_dev:
            errors.append("development dependency set differs from factory allowlist")

        app = texts["src/App.tsx"]
        main = texts["src/main.tsx"]
        styles = texts["src/styles.css"]
        domain = texts["server/domain.mjs"]
        db = texts["server/db.mjs"]
        server = texts["server/server.mjs"]
        domain_test = texts["tests/domain.test.mjs"]
        api_test = texts["tests/api.test.mjs"]
        readme = texts["README.md"]
        all_generated = "\n".join([app, main, styles, domain, db, server, domain_test, api_test, readme]).lower()

        if "react" not in main.lower() or "app" not in main.lower():
            errors.append("React entrypoint does not mount the product App")
        if "/api/" not in app:
            errors.append("frontend must use the real same-origin REST API")
        if "node:sqlite" not in db.lower() or "databasesync" not in db.lower():
            errors.append("SQLite persistence must use node:sqlite DatabaseSync")
        if "createappserver" not in server.lower():
            errors.append("server must export createAppServer for integration testing")
        if "/api/health" not in server:
            errors.append("server must expose /api/health")
        if "node:test" not in domain_test.lower() or "node:assert" not in domain_test.lower():
            errors.append("domain tests must use node:test and node:assert")
        if "node:test" not in api_test.lower() or "node:assert" not in api_test.lower():
            errors.append("API tests must use node:test and node:assert")
        if "createappserver" not in api_test.lower():
            errors.append("API tests must execute the real HTTP server")

        domain_assertions = len(re.findall(r"\bassert(?:\.|\()", domain_test))
        api_assertions = len(re.findall(r"\bassert(?:\.|\()", api_test))
        if domain_assertions < 6:
            errors.append("domain test suite must contain at least 6 assertions")
        if api_assertions < 6:
            errors.append("API integration suite must contain at least 6 assertions")

        if len(app.strip()) < 2500:
            errors.append("App.tsx is too small for a production-style multi-workflow product")
        if len(domain.strip()) < 1800:
            errors.append("domain engine is too small for a substantial product")
        if len(db.strip()) < 1400:
            errors.append("database layer is too small for durable product persistence")
        if len(server.strip()) < 2600:
            errors.append("REST server is too small for a substantial API")
        if len(styles.strip()) < 1800:
            errors.append("frontend styling is too small for a portfolio-grade responsive interface")
        if len(readme.strip()) < 2200:
            errors.append("README.md is too small for production-style documentation")

        for marker in ("todo", "coming soon", "lorem ipsum", "not implemented"):
            if marker in all_generated:
                errors.append(f"unfinished marker found: {marker}")

        forbidden = (
            "child_process", "exec(", "execsync(", "spawn(", "spawnsync(",
            "eval(", "new function(", "websocket(", "worker_threads", "node:vm",
        )
        for token in forbidden:
            if token in all_generated:
                errors.append(f"forbidden dynamic/system behavior found: {token}")

        # Public-network URLs in generated source are disallowed. Localhost in tests/docs
        # is permitted because the API integration suite must call its ephemeral server.
        for match in re.findall(r"https?://[^\s\"'`)>]+", "\n".join(texts.values()), flags=re.IGNORECASE):
            lower = match.lower()
            if not any(host in lower for host in ("127.0.0.1", "localhost")):
                errors.append(f"public network URL is not allowed in generated product: {match[:120]}")
                break

        # Frontend fetches must target same-origin /api paths, not arbitrary URLs.
        for target in re.findall(r"fetch\s*\(\s*([\"'`])(.+?)\1", app, flags=re.DOTALL):
            value = target[1].strip()
            if not value.startswith("/api/"):
                errors.append("frontend fetch target must be same-origin /api/...")
                break

    results = [_static_result(errors, "full-stack architecture, tests, persistence, local-only policy and documentation passed")]
    if errors:
        return results

    npm = shutil.which("npm")
    node = shutil.which("node")
    docker = shutil.which("docker")
    if not npm or not node:
        results.append(ValidationResult(
            ["npm", "test"], 1, "FAIL", "DEPENDENCY_FAILURE", "Node.js/npm is unavailable"
        ))
        return results

    try:
        install = _run(project, ["npm", "install", "--ignore-scripts", "--no-audit", "--no-fund"], "build", timeout=300)
        results.append(install)
        if install.exit_code != 0:
            return results

        results.append(_run(project, ["npm", "test"], "test", timeout=180))
        results.append(_run(project, ["npm", "run", "build"], "build", timeout=240))

        if docker:
            results.append(_run(project, ["docker", "build", "-t", "factory-validation:latest", "."], "build", timeout=420))
        else:
            results.append(ValidationResult(
                ["docker", "build"], 1, "FAIL", "DEPENDENCY_FAILURE", "docker executable is unavailable"
            ))
    finally:
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
            errors.append("app.js must consume ProductCore")
        if "core.js" not in tests.lower():
            errors.append("test-core.js must execute the real core.js engine")
        assertion_signals = sum(tests.lower().count(token) for token in ("assert", "throw new error", "expect("))
        if assertion_signals < 5:
            errors.append("test-core.js must contain at least 5 assertions/checks")
        if len(css.strip()) < 1000 or len(js.strip()) < 1200 or len(core.strip()) < 1000 or len(readme.strip()) < 1200:
            errors.append("legacy browser product is below the substantial-product size gate")
        for marker in ("todo", "coming soon", "lorem ipsum"):
            if marker in lower_all:
                errors.append(f"unfinished marker found: {marker}")
        for token in ("eval(", "new function(", "document.write(", "websocket("):
            if token in lower_all:
                errors.append(f"forbidden dynamic behavior found: {token}")

    results = [_static_result(errors, "legacy high-end browser architecture and functional-test contract passed")]
    if errors:
        return results
    node = shutil.which("node")
    if node:
        for name in ("core.js", "app.js", "test-core.js"):
            results.append(_run(project, ["node", "--check", name], "build"))
        results.append(_run(project, ["node", "test-core.js"], "test"))
    else:
        results.append(ValidationResult(["node", "test-core.js"], 1, "FAIL", "DEPENDENCY_FAILURE", "node unavailable"))
    return results


def validate(project: Path) -> list[ValidationResult]:
    if (project / "package.json").is_file() and (project / "src" / "App.tsx").is_file():
        return _validate_fullstack(project)
    if (project / "index.html").is_file():
        return _validate_legacy_web(project)
    commands = [
        (["python", "-m", "unittest", "discover", "-s", "tests", "-v"], "test"),
        (["python", "-m", "compileall", "-q", "src", "tests"], "build"),
    ]
    return [_run(project, command, kind) for command, kind in commands]
