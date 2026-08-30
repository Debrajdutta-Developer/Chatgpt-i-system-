"""Gemini discovery/generation for hard systems-engineering projects.

Model output is untrusted. The factory owns allowed languages, canonical paths,
CI scaffolds and validation commands. Gemini may vary JSON wrapping or file paths;
we normalize only to a fixed allowlisted contract and never write unexpected paths.
"""
from __future__ import annotations

import json
import os
import re
import time
import urllib.error
import urllib.request
from pathlib import Path, PurePosixPath

from .models import Idea

MODEL = os.getenv("GEMINI_MODEL", "gemini-3.5-flash").strip()
FALLBACK_MODELS = tuple(
    x.strip() for x in os.getenv(
        "GEMINI_FALLBACK_MODELS",
        "gemini-3.5-flash-lite,gemini-3.1-flash-lite,gemini-2.5-flash-lite,gemini-2.5-flash",
    ).split(",") if x.strip()
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
        "README.md", "src/engine.h", "src/engine.c", "src/main.c",
        "tests/test_engine.c", "tests/test_integration.c",
    },
    "systems-python": {
        "README.md", "src/engine.py", "src/cli.py",
        "tests/test_engine.py", "tests/test_integration.py",
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
on: [push, pull_request]
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
on: [push, pull_request]
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
on: [push, pull_request]
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
    req = urllib.request.Request(
        f"{API_BASE}/{model}:generateContent",
        data=body,
        headers={"Content-Type": "application/json", "x-goog-api-key": API_KEY},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=120) as response:
        return json.loads(response.read().decode("utf-8"))


def _request(prompt: str, temperature: float = 0.5):
    if not API_KEY:
        raise RuntimeError("GEMINI_API_KEY is not configured")
    body = json.dumps({
        "contents": [{"role": "user", "parts": [{"text": prompt}]}],
        "generationConfig": {
            "temperature": temperature,
            "responseMimeType": "application/json",
        },
    }).encode("utf-8")
    failures: list[str] = []
    for model in _models():
        for attempt in range(1, MAX_ATTEMPTS_PER_MODEL + 1):
            try:
                payload = _request_once(model, body)
                text = payload["candidates"][0]["content"]["parts"][0]["text"]
                parsed = json.loads(text)
                if not isinstance(parsed, (dict, list)):
                    raise RuntimeError(f"Gemini model {model} returned non-container JSON")
                return parsed
            except urllib.error.HTTPError as exc:
                detail = exc.read().decode("utf-8", errors="replace")[:500]
                failures.append(f"{model} attempt {attempt}: HTTP {exc.code}: {detail}")
                if exc.code not in RETRYABLE_HTTP_CODES:
                    break
            except (urllib.error.URLError, TimeoutError) as exc:
                failures.append(f"{model} attempt {attempt}: network failure: {exc}")
            except (KeyError, IndexError, TypeError, json.JSONDecodeError, RuntimeError) as exc:
                failures.append(f"{model} attempt {attempt}: invalid structured output: {exc}")
                break
            if attempt < MAX_ATTEMPTS_PER_MODEL:
                time.sleep(min(2 ** (attempt - 1), 4))
    raise RuntimeError("All Gemini model attempts failed: " + " | ".join(failures[-10:]))


def _slug(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")[:64]


def _idea_items(payload) -> list:
    if isinstance(payload, dict):
        raw = payload.get("ideas")
    elif isinstance(payload, list):
        if len(payload) == 1 and isinstance(payload[0], dict) and isinstance(payload[0].get("ideas"), list):
            raw = payload[0]["ideas"]
        else:
            raw = payload
    else:
        raw = None
    if not isinstance(raw, list):
        raise RuntimeError("Gemini idea response is missing ideas[]")
    return raw


def _files_map(payload) -> dict:
    candidate = payload
    if isinstance(candidate, list) and len(candidate) == 1 and isinstance(candidate[0], dict):
        candidate = candidate[0]
    if not isinstance(candidate, dict):
        raise RuntimeError("Gemini build response is not a JSON object")
    files = candidate.get("files")
    if isinstance(files, list):
        converted = {}
        for item in files:
            if isinstance(item, dict) and isinstance(item.get("path"), str) and isinstance(item.get("content"), str):
                converted[item["path"]] = item["content"]
        files = converted
    if not isinstance(files, dict):
        raise RuntimeError("Gemini build response is missing files")
    return files


def _canonicalize_files(files: dict, expected: set[str]) -> dict[str, str]:
    """Map harmless Gemini path variations to fixed trusted paths.

    Traversal/absolute paths are rejected before any cleanup. Exact canonical paths
    win; otherwise an expected path may be matched by a unique basename. Model paths
    are never used as write destinations.
    """
    clean: dict[str, str] = {}
    normalized: dict[str, str] = {}
    for raw_path, content in files.items():
        if not isinstance(raw_path, str) or not isinstance(content, str):
            continue
        path = raw_path.replace("\\", "/").strip()
        parts = PurePosixPath(path).parts
        if not path or path.startswith("/") or ".." in parts:
            raise RuntimeError(f"unsafe Gemini file path rejected: {raw_path}")
        while path.startswith("./"):
            path = path[2:]
        if not path:
            raise RuntimeError(f"unsafe Gemini file path rejected: {raw_path}")
        normalized[path] = content

    for canonical in expected:
        if canonical in normalized:
            clean[canonical] = normalized[canonical]
            continue
        basename = PurePosixPath(canonical).name
        matches = [content for path, content in normalized.items() if PurePosixPath(path).name == basename]
        if len(matches) == 1:
            clean[canonical] = matches[0]

    missing = sorted(expected - set(clean))
    if missing:
        received = sorted(normalized)[:20]
        raise RuntimeError(
            "Gemini build is missing canonical files: " + ", ".join(missing)
            + "; received paths: " + ", ".join(received)
        )
    return clean


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
        for item in history[-100:] if isinstance(item, dict)
    ]
    prompt = f"""
You are the principal systems engineer for an autonomous software factory.
Propose {count} DIFFERENT HARD or EXTREME projects comparable to a Java mini-Redis,
private search engine, small database/storage engine, safe local learning agent,
or C compiler/interpreter. Reject CRUD/SaaS dashboards, calculators, note apps,
API wrappers, landing pages and superficial demos.

Each project must implement its central algorithm/protocol itself, run locally,
use exactly one of `Java 21`, `C11`, `Python 3.12`, have at least 8 connected
features, and define deterministic unit/integration verification including malformed
or edge input. Prefer compiler/VM, database/storage/query, cache/protocol, search/index,
message queue/event log, parser/static analyzer, safe local agent, scheduler or similar
systems infrastructure. No malware, credential theft, surveillance, exploit/evasion,
destructive automation or autonomous public-network actions. A learning agent may
adapt only from explicit local feedback and may not self-modify or execute shell code.
Do not repeat PREVIOUS_RELEASES.

PREVIOUS_RELEASES={json.dumps(previous, ensure_ascii=False)}

Return only JSON: {{"ideas":[{{"name":"...","slug":"...","target_user":"...",
"problem":"...","pain":"...","solution":"...","verification":"...",
"category":"compiler|database|cache-protocol|search-engine|agent-engine|systems-tool|other-systems",
"keywords":["systems-engineering","..."],"major_features":["8 or more features"],
"technology":"Java 21|C11|Python 3.12"}}]}}
"""
    raw_ideas = _idea_items(_request(prompt, temperature=0.72))
    ideas: list[Idea] = []
    for raw in raw_ideas[:count]:
        if not isinstance(raw, dict):
            continue
        raw_keywords = raw.get("keywords", [])
        raw_features = raw.get("major_features", [])
        if not isinstance(raw_keywords, list) or not isinstance(raw_features, list):
            continue
        slug = _slug(str(raw.get("slug") or raw.get("name") or ""))
        keywords = tuple(str(x).strip() for x in raw_keywords if str(x).strip())
        features = tuple(str(x).strip() for x in raw_features if str(x).strip())
        technology = str(raw.get("technology") or "").strip()
        fields = [raw.get(k) for k in ("name", "target_user", "problem", "pain", "solution", "verification", "category")]
        if not slug or technology not in TECH_TO_PROFILE or len(features) < 8:
            continue
        if not all(isinstance(x, str) and x.strip() for x in fields):
            continue
        if "systems-engineering" not in {k.lower() for k in keywords}:
            keywords = ("systems-engineering", *keywords)
        ideas.append(Idea(
            str(raw["name"]).strip(), slug, str(raw["target_user"]).strip(),
            str(raw["problem"]).strip(), str(raw["pain"]).strip(), str(raw["solution"]).strip(),
            str(raw["verification"]).strip(), str(raw["category"]).strip(), keywords, features, technology,
        ))
    if not ideas:
        raise RuntimeError("Gemini produced no valid hard systems-engineering candidates")
    return ideas


def _profile(idea: Idea) -> str:
    if idea.technology not in TECH_TO_PROFILE:
        raise RuntimeError(f"unsupported systems technology: {idea.technology}")
    return TECH_TO_PROFILE[idea.technology]


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
    expected = sorted(PROFILE_FILES[profile])
    common = f"""
Build one complete runnable hard systems project for:
{json.dumps(idea.to_dict(), ensure_ascii=False)}

Return ONLY JSON with a top-level `files` object. The exact canonical file keys are:
{json.dumps(expected)}
Do not add, rename or omit canonical files.
Implement every major feature with real standard-library-only code. Central parsing,
indexing, storage, protocol, state-machine or execution logic must be implemented in
this repository. Unit and integration tests must execute the real engine and each suite
must have at least 10 explicit, independent assertion/check invocations so the factory
can verify test depth mechanically. Include normal, malformed, boundary, persistence or
state-transition cases where relevant. No TODOs, fake results, shell/subprocess execution,
credential access, public-network calls, self-modifying code or external packages.
README.md MUST be at least 3200 characters and contain explicit sections named
Architecture, Algorithms and Data Structures, Invariants, Build and Run, Testing,
Security and Privacy, Performance Characteristics, and Limitations. Do not pad it with
repetition; document the actual generated implementation and its honest scope.
REPAIR_FEEDBACK={json.dumps(feedback, ensure_ascii=False)}
"""
    if profile == "systems-java":
        return common + """
All Java files use package `factory`. Core.java contains the reusable engine. Main.java
provides the real CLI/server. For cache/protocol projects, a localhost TCP server and
bounded protocol parsing are appropriate. CoreTest and IntegrationTest are executable
classes with public static void main. In EACH test file use explicit calls whose names
start with assert/check/verify/expect/require at least 10 times; do not hide many cases
behind a single loop or one helper invocation. Prefer a small `check(boolean,String)`
helper and call it independently for each tested behavior. Java 21, no preview.
When writing Java regex literals, double-escape backslashes correctly for Java source.
Avoid hand-built JSON string literals that require complex nested quoting; use simple
plain-text sample values unless JSON itself is part of the feature under test.
"""
    if profile == "systems-c":
        return common + """
Portable C11. engine.h defines API/ownership/error contracts; engine.c implements the
engine; main.c is a real CLI. Tests each contain main, return nonzero on failure, free
owned allocations, and compile with -Wall -Wextra -Werror -pedantic. In EACH test file
make at least 10 explicit calls to assert/check/verify/expect/require-style helpers.
No system/popen.
"""
    return common + """
Python 3.12 standard library only. engine.py contains the real engine; cli.py uses
argparse and calls it. Both unittest files exercise real state/persistence and each must
contain at least 10 explicit `self.assert...(...)` or bare assert checks. Private search
must implement tokenization/inverted index/ranking locally. A safe learning agent may
implement local memory/scoring/feedback updates but no arbitrary code execution.
"""


def _preflight_check_count(text: str) -> int:
    lower = text.lower()
    return (
        len(re.findall(r"\bassert\w*\s*\(", lower))
        + len(re.findall(r"\bcheck\s*\(", lower))
        + len(re.findall(r"\bverify\w*\s*\(", lower))
        + len(re.findall(r"\bexpect\w*\s*\(", lower))
        + len(re.findall(r"\brequire\w*\s*\(", lower))
        + lower.count("assert ")
    )


def _preflight_files(files: dict[str, str], profile: str) -> list[str]:
    """Catch deterministic contract defects before consuming a repair attempt."""
    errors: list[str] = []
    readme = files.get("README.md", "")
    if len(readme.strip()) < 3000:
        errors.append("README.md must be at least 3000 characters before validation")
    for section in ("architecture", "test", "limitation"):
        if section not in readme.lower():
            errors.append(f"README.md preflight is missing {section} documentation")

    if profile == "systems-java":
        core = files.get("src/main/java/factory/Core.java", "")
        main = files.get("src/main/java/factory/Main.java", "")
        unit = files.get("src/test/java/factory/CoreTest.java", "")
        integ = files.get("src/test/java/factory/IntegrationTest.java", "")
        if len(core.strip()) < 2600:
            errors.append("Core.java must be at least 2600 characters")
        if len(main.strip()) < 900 or "public static void main" not in main:
            errors.append("Main.java must be a substantial runnable entrypoint")
        for name, text in (("CoreTest.java", unit), ("IntegrationTest.java", integ)):
            if "public static void main" not in text:
                errors.append(f"{name} must expose public static void main")
            if _preflight_check_count(text) < 10:
                errors.append(f"{name} must contain at least 10 explicit check/assert invocations")

    elif profile == "systems-c":
        if len(files.get("src/engine.c", "").strip()) < 3000:
            errors.append("engine.c must be at least 3000 characters")
        if len(files.get("src/engine.h", "").strip()) < 500:
            errors.append("engine.h must be at least 500 characters")
        main = files.get("src/main.c", "")
        if len(main.strip()) < 700 or "main(" not in main:
            errors.append("main.c must be a substantial runnable CLI")
        for name in ("tests/test_engine.c", "tests/test_integration.c"):
            text = files.get(name, "")
            if "main(" not in text:
                errors.append(f"{name} must provide main()")
            if _preflight_check_count(text) < 10:
                errors.append(f"{name} must contain at least 10 explicit check/assert invocations")

    elif profile == "systems-python":
        if len(files.get("src/engine.py", "").strip()) < 3000:
            errors.append("engine.py must be at least 3000 characters")
        cli = files.get("src/cli.py", "")
        if len(cli.strip()) < 800 or "argparse" not in cli:
            errors.append("cli.py must be a substantial argparse CLI")
        for name in ("tests/test_engine.py", "tests/test_integration.py"):
            text = files.get(name, "")
            if "unittest" not in text:
                errors.append(f"{name} must use unittest")
            if _preflight_check_count(text) < 10:
                errors.append(f"{name} must contain at least 10 explicit assertions")
    return errors


def build_project(idea: Idea, destination: Path, repair_feedback: list[str] | None = None) -> list[str]:
    feedback = list(repair_feedback or [])
    profile = _profile(idea)
    expected = PROFILE_FILES[profile]
    last_error: RuntimeError | None = None

    # Format/depth defects are cheap to detect before compilation and should not burn
    # the orchestrator's bounded correctness-repair budget. Give the model up to three
    # focused generation attempts to satisfy the immutable structural contract first.
    for schema_attempt in range(3):
        prompt = _implementation_contract(idea, profile, feedback)
        try:
            files = _canonicalize_files(_files_map(_request(prompt, temperature=0.18)), expected)
            preflight = _preflight_files(files, profile)
            if preflight:
                raise RuntimeError("preflight contract failure: " + "; ".join(preflight))
            break
        except RuntimeError as exc:
            last_error = exc
            feedback = [
                *feedback[-6:],
                str(exc),
                "Preserve every previously requested feature while fixing all structural preflight defects.",
                "Return every canonical file key exactly as requested.",
            ]
    else:
        raise RuntimeError(f"generation contract failure after preflight retries: {last_error}")

    destination.mkdir(parents=True, exist_ok=True)
    created: list[str] = []
    for relative in sorted(expected):
        content = files[relative]
        if len(content.strip()) < 120:
            raise RuntimeError(f"Gemini returned invalid or too-small {relative}")
        path = destination / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content.rstrip() + "\n", encoding="utf-8")
        created.append(relative)
    created.extend(_write_trusted_scaffold(destination, idea, profile))
    return sorted(created)
