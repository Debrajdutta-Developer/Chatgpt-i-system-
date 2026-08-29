"""Gemini-backed discovery and generation for hard systems-engineering projects.

Gemini output is untrusted source data. Build/test commands, CI workflows, allowed
languages and generated paths are fixed by factory code. The provider deliberately
prefers compiler, storage, search, cache/protocol and safe local-agent projects over
ordinary CRUD/SaaS apps.
"""
from __future__ import annotations

import json
import os
import re
import time
import urllib.error
import urllib.request
from pathlib import Path

from .models import Idea

MODEL = os.getenv("GEMINI_MODEL", "gemini-3.5-flash").strip()
FALLBACK_MODELS = tuple(
    item.strip()
    for item in os.getenv(
        "GEMINI_FALLBACK_MODELS",
        "gemini-3.5-flash-lite,gemini-3.1-flash-lite,gemini-2.5-flash-lite,gemini-2.5-flash",
    ).split(",")
    if item.strip()
)
API_KEY = os.getenv("GEMINI_API_KEY", "").strip()
API_BASE = "https://generativelanguage.googleapis.com/v1beta/models"
RETRYABLE_HTTP_CODES = {429, 500, 502, 503, 504}
MAX_ATTEMPTS_PER_MODEL = max(1, int(os.getenv("GEMINI_RETRY_ATTEMPTS", "2")))

TECH_TO_PROFILE = {
    "Java 21": "systems-java",
    "C11": "systems-c",
    "Python 3.12": "systems-python",
}

PROFILE_FILES = {
    "systems-java": {
        "README.md",
        "src/main/java/factory/Core.java",
        "src/main/java/factory/Main.java",
        "src/test/java/factory/CoreTest.java",
        "src/test/java/factory/IntegrationTest.java",
    },
    "systems-c": {
        "README.md",
        "src/engine.h",
        "src/engine.c",
        "src/main.c",
        "tests/test_engine.c",
        "tests/test_integration.c",
    },
    "systems-python": {
        "README.md",
        "src/engine.py",
        "src/cli.py",
        "tests/test_engine.py",
        "tests/test_integration.py",
    },
}

COMMON_GITIGNORE = """build/
__pycache__/
*.py[cod]
*.class
*.o
*.out
*.db
*.sqlite
*.sqlite3
.factory-work/
"""

CI_BY_PROFILE = {
    "systems-java": """name: CI
on:
  push:
  pull_request:
permissions:
  contents: read
jobs:
  verify:
    runs-on: ubuntu-latest
    timeout-minutes: 15
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-java@v4
        with:
          distribution: temurin
          java-version: '21'
      - run: mkdir -p build/classes
      - run: javac -Xlint:all -Werror -d build/classes src/main/java/factory/*.java src/test/java/factory/*.java
      - run: java -ea -cp build/classes factory.CoreTest
      - run: java -ea -cp build/classes factory.IntegrationTest
""",
    "systems-c": """name: CI
on:
  push:
  pull_request:
permissions:
  contents: read
jobs:
  verify:
    runs-on: ubuntu-latest
    timeout-minutes: 15
    steps:
      - uses: actions/checkout@v4
      - run: mkdir -p build
      - run: cc -std=c11 -Wall -Wextra -Werror -pedantic -I src src/engine.c tests/test_engine.c -o build/test_engine
      - run: ./build/test_engine
      - run: cc -std=c11 -Wall -Wextra -Werror -pedantic -I src src/engine.c tests/test_integration.c -o build/test_integration
      - run: ./build/test_integration
      - run: cc -std=c11 -Wall -Wextra -Werror -pedantic -I src src/engine.c src/main.c -o build/app
""",
    "systems-python": """name: CI
on:
  push:
  pull_request:
permissions:
  contents: read
jobs:
  verify:
    runs-on: ubuntu-latest
    timeout-minutes: 15
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - run: python -m compileall -q src tests
      - run: python -m unittest discover -s tests -v
""",
}


def available() -> bool:
    return bool(API_KEY)


def _models() -> tuple[str, ...]:
    ordered: list[str] = []
    for name in (MODEL, *FALLBACK_MODELS):
        if name and name not in ordered:
            ordered.append(name)
    return tuple(ordered)


def _request_once(model: str, body: bytes) -> dict:
    url = f"{API_BASE}/{model}:generateContent"
    request = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json", "x-goog-api-key": API_KEY},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=120) as response:
        return json.loads(response.read().decode("utf-8"))


def _request(prompt: str, temperature: float = 0.5) -> dict:
    if not API_KEY:
        raise RuntimeError("GEMINI_API_KEY is not configured")

    body = json.dumps({
        "contents": [{"role": "user", "parts": [{"text": prompt}]}],
        "generationConfig": {"temperature": temperature, "responseMimeType": "application/json"},
    }).encode("utf-8")

    failures: list[str] = []
    for model in _models():
        for attempt in range(1, MAX_ATTEMPTS_PER_MODEL + 1):
            try:
                payload = _request_once(model, body)
                try:
                    text = payload["candidates"][0]["content"]["parts"][0]["text"]
                    return json.loads(text)
                except (KeyError, IndexError, TypeError, json.JSONDecodeError) as exc:
                    raise RuntimeError(f"Gemini model {model} returned invalid structured output") from exc
            except urllib.error.HTTPError as exc:
                detail = exc.read().decode("utf-8", errors="replace")[:500]
                failures.append(f"{model} attempt {attempt}: HTTP {exc.code}: {detail}")
                if exc.code not in RETRYABLE_HTTP_CODES:
                    break
                if attempt < MAX_ATTEMPTS_PER_MODEL:
                    time.sleep(min(2 ** (attempt - 1), 4))
            except (urllib.error.URLError, TimeoutError) as exc:
                failures.append(f"{model} attempt {attempt}: network failure: {exc}")
                if attempt < MAX_ATTEMPTS_PER_MODEL:
                    time.sleep(min(2 ** (attempt - 1), 4))
            except RuntimeError as exc:
                failures.append(str(exc))
                break

    summary = " | ".join(failures[-10:])
    raise RuntimeError(f"All Gemini model attempts failed: {summary}")


def _slug(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")[:64]


def discover_ideas(history: list[dict], count: int = 5) -> list[Idea]:
    previous = [
        {
            "name": item.get("name"),
            "problem": item.get("problem"),
            "purpose": item.get("purpose") or item.get("solution"),
            "category": item.get("category"),
            "features": item.get("major_features"),
            "technology": item.get("technology"),
        }
        for item in history[-100:]
    ]
    prompt = f"""
You are the principal systems engineer for an autonomous software factory.
Propose {count} DIFFERENT HARD or EXTREME engineering projects. The standard is projects like:
- a Java mini Redis-compatible cache/server,
- a private local search engine with its own index/ranker,
- a small database/storage engine with pages/indexes/query execution,
- a safe local learning/planning agent with memory + evaluation + feedback,
- a C compiler/interpreter with lexer, parser, AST, semantic checks and code generation.

We want projects that teach or demonstrate how important infrastructure works internally. Reject ordinary SaaS, CRUD apps, dashboards, landing pages, calculators, converters, to-do/note apps, wrappers around APIs, thin AI chat UIs, and superficial demos.

Preferred families include compiler/interpreter/VM, database/storage/query engine, cache/protocol server, search/index/ranking engine, message queue/event log, version-control/build system, scheduler, parser/static analyzer, safe local agent/evaluation engine, data-structure engine, observability/log-processing engine, or similarly deep developer infrastructure.

Hard requirements for every candidate:
- It must implement its central algorithm/protocol itself rather than delegating the hard part to a library/service.
- It must be useful and runnable locally with no paid/cloud dependency.
- Choose technology EXACTLY from: `Java 21`, `C11`, `Python 3.12`.
- Choose the language because it fits the engineering problem, not randomly.
- At least 8 concrete connected major features.
- A deterministic verification plan with unit + integration behavior and malformed/edge cases.
- Include persistence, protocol/parsing, indexing, state-machine, execution, evaluation, or another genuine engine-level concern where relevant.
- No fake benchmarks, fake users, fake integrations, fabricated test results, or claims of live market research.
- No malware, credential theft, exploit tooling, surveillance, evasion, destructive automation, weapons, gambling, adult content, or autonomous public-network actions.
- A safe agent may learn only from local user-supplied feedback/data and must not self-modify code, execute shell commands, or act on external services.
- Do not repeat or lightly reskin PREVIOUS_RELEASES.
- Rotate project families and languages compared with recent releases.

PREVIOUS_RELEASES={json.dumps(previous, ensure_ascii=False)}

Return ONLY JSON with this schema:
{{"ideas":[{{
  "name":"...",
  "slug":"lowercase-kebab-case",
  "target_user":"...",
  "problem":"...",
  "pain":"...",
  "solution":"...",
  "verification":"unit + integration checks for real engine behavior, malformed input and persistence/state cases",
  "category":"compiler|database|cache-protocol|search-engine|agent-engine|systems-tool|other-systems",
  "keywords":["systems-engineering","..."],
  "major_features":["...","...","...","...","...","...","...","..."],
  "technology":"Java 21|C11|Python 3.12"
}}]}}
"""
    payload = _request(prompt, temperature=0.72)
    raw_ideas = payload.get("ideas")
    if not isinstance(raw_ideas, list):
        raise RuntimeError("Gemini idea response is missing ideas[]")

    ideas: list[Idea] = []
    for raw in raw_ideas[:count]:
        if not isinstance(raw, dict):
            continue
        slug = _slug(str(raw.get("slug") or raw.get("name") or ""))
        keywords = tuple(str(x).strip() for x in raw.get("keywords", []) if str(x).strip())
        features = tuple(str(x).strip() for x in raw.get("major_features", []) if str(x).strip())
        technology = str(raw.get("technology") or "").strip()
        fields = [raw.get(k) for k in ("name", "target_user", "problem", "pain", "solution", "verification", "category")]
        if (
            not slug
            or technology not in TECH_TO_PROFILE
            or not all(isinstance(x, str) and x.strip() for x in fields)
            or len(features) < 8
        ):
            continue
        if "systems-engineering" not in {k.lower() for k in keywords}:
            keywords = ("systems-engineering", *keywords)
        ideas.append(Idea(
            str(raw["name"]).strip(), slug, str(raw["target_user"]).strip(),
            str(raw["problem"]).strip(), str(raw["pain"]).strip(), str(raw["solution"]).strip(),
            str(raw["verification"]).strip(), str(raw["category"]).strip(), keywords, features,
            technology,
        ))
    if not ideas:
        raise RuntimeError("Gemini produced no valid hard systems-engineering candidates")
    return ideas


def _profile(idea: Idea) -> str:
    try:
        return TECH_TO_PROFILE[idea.technology]
    except KeyError as exc:
        raise RuntimeError(f"unsupported systems technology: {idea.technology}") from exc


def _write_trusted_scaffold(destination: Path, idea: Idea, profile: str) -> list[str]:
    metadata = idea.to_dict()
    metadata.update({
        "factory_profile": profile,
        "difficulty": "HARD_OR_EXTREME",
        "truth_policy": "Only validated behavior may be claimed as working.",
    })
    trusted = {
        "project.json": json.dumps(metadata, indent=2) + "\n",
        ".gitignore": COMMON_GITIGNORE,
        ".github/workflows/ci.yml": CI_BY_PROFILE[profile],
    }
    for relative, content in trusted.items():
        path = destination / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    return sorted(trusted)


def _implementation_contract(idea: Idea, profile: str, feedback: list[str]) -> str:
    common = f"""
You are the senior systems implementation team of an autonomous software factory.
Build one COMPLETE, runnable engineering project for this selected idea:
{json.dumps(idea.to_dict(), ensure_ascii=False)}

This is a REAL ENGINE gate. A mock UI, pseudocode, hard-coded success path, or README-only project is failure.
The factory owns project.json, .gitignore and CI. You generate ONLY the exact source/test/doc paths requested below.

Universal rules:
- Return ONLY JSON matching the requested file map exactly.
- Implement ALL major_features with real code or fail honestly; never fake an unimplemented feature.
- The central algorithm/protocol/parser/index/state machine must be implemented by this repository, not outsourced to a third-party service.
- Use only the selected language's standard library. No downloaded packages or public-network dependency.
- Include defensive validation, malformed-input behavior, deterministic state transitions, and meaningful error messages.
- Unit tests and integration tests must execute the REAL engine, not duplicated test-only logic.
- Each test suite must contain at least 8 meaningful assertions/checks. Include normal, boundary, malformed and state/persistence/integration cases where relevant.
- No TODO, placeholder, lorem ipsum, fake metric, fabricated benchmark, fake test claim, hard-coded success screen, or unfinished control.
- No shell/subprocess execution, self-modifying code, credential access, remote analytics, trackers, public-network calls or autonomous external actions.
- A local agent may adapt rankings/policies/memory from explicit local feedback, but may not execute arbitrary code or change its own source.
- README must be substantial and explain architecture, algorithms/data structures, invariants, supported/unsupported behavior, exact build/run/test commands, examples, verification strategy, performance characteristics without fabricated numbers, security/privacy properties and honest limitations.
- If REPAIR_FEEDBACK is non-empty, fix every reported issue instead of hiding/removing the affected feature.

REPAIR_FEEDBACK={json.dumps(feedback, ensure_ascii=False)}
"""

    if profile == "systems-java":
        return common + """
Java 21 contract:
- Package must be exactly `factory` for all Java files.
- `Core.java` contains the substantial reusable engine/data structures/protocol logic; do not put the real implementation only in Main.
- `Main.java` provides a real CLI or local server entrypoint suitable for the product. If the idea is a Redis/cache/protocol server, implement a localhost-capable TCP server and protocol parsing with bounded request sizes/timeouts. Otherwise provide a useful CLI over Core.
- `CoreTest.java` is a dependency-free executable test class with `public static void main(String[] args)` and >=8 real assertions/checks.
- `IntegrationTest.java` is another executable test class that exercises end-to-end behavior. Local loopback sockets are allowed when the product is a server; public network access is not.
- Avoid preview Java features and external libraries.

Return exactly:
{"files":{
 "README.md":"...",
 "src/main/java/factory/Core.java":"...",
 "src/main/java/factory/Main.java":"...",
 "src/test/java/factory/CoreTest.java":"...",
 "src/test/java/factory/IntegrationTest.java":"..."
}}
"""

    if profile == "systems-c":
        return common + """
C11 contract:
- Portable C11 only; compile cleanly under `-Wall -Wextra -Werror -pedantic`.
- `engine.h` defines the public API and explicit ownership/error contracts.
- `engine.c` implements the substantial core algorithm/data structures/parser/storage/execution logic.
- `main.c` provides a useful CLI that drives the real engine.
- Tests must avoid undefined behavior, check return/error paths, and free owned allocations.
- `test_engine.c` and `test_integration.c` each have a `main` and >=8 meaningful checks that return non-zero on failure.
- No `system`, `popen`, shell execution, dynamic code loading or public-network behavior.

Return exactly:
{"files":{
 "README.md":"...",
 "src/engine.h":"...",
 "src/engine.c":"...",
 "src/main.c":"...",
 "tests/test_engine.c":"...",
 "tests/test_integration.c":"..."
}}
"""

    if profile == "systems-python":
        return common + """
Python 3.12 contract:
- Standard library only.
- `src/engine.py` contains the substantial reusable indexing/query/storage/agent/planning/domain engine.
- `src/cli.py` provides a real argparse-based CLI that invokes engine.py; no duplicated fake logic.
- `tests/test_engine.py` and `tests/test_integration.py` use unittest and exercise persistence/state across multiple operations. Each file must contain >=8 assertions/checks.
- For private search, index local user-supplied text/files and implement tokenization/inverted index/ranking yourself.
- For a safe learning agent, implement explicit local memory, scoring/evaluation and feedback-driven policy/ranking updates; do not claim neural training unless it is actually implemented.
- No subprocess, os.system, eval/exec, arbitrary plugin loading or public-network access.

Return exactly:
{"files":{
 "README.md":"...",
 "src/engine.py":"...",
 "src/cli.py":"...",
 "tests/test_engine.py":"...",
 "tests/test_integration.py":"..."
}}
"""

    raise RuntimeError(f"unknown systems profile: {profile}")


def build_project(idea: Idea, destination: Path, repair_feedback: list[str] | None = None) -> list[str]:
    feedback = repair_feedback or []
    profile = _profile(idea)
    prompt = _implementation_contract(idea, profile, feedback)
    payload = _request(prompt, temperature=0.18)
    files = payload.get("files")
    expected = PROFILE_FILES[profile]
    if not isinstance(files, dict):
        raise RuntimeError("Gemini build response is missing files")
    if set(files) != expected:
        raise RuntimeError(f"Gemini build returned unexpected or missing {profile} source paths")

    destination.mkdir(parents=True, exist_ok=True)
    created: list[str] = []
    for relative in sorted(expected):
        content = files.get(relative)
        if not isinstance(content, str) or len(content.strip()) < 120:
            raise RuntimeError(f"Gemini returned invalid or too-small {relative}")
        path = destination / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content.rstrip() + "\n", encoding="utf-8")
        created.append(relative)

    created.extend(_write_trusted_scaffold(destination, idea, profile))
    return sorted(created)
