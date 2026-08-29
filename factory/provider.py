"""Gemini-backed discovery and production-style full-stack generation.

Gemini output is treated as untrusted source data. Dependency manifests, CI,
container configuration and validation commands are factory-owned and fixed.
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

GENERATED_FILES = {
    "README.md",
    "index.html",
    "src/main.tsx",
    "src/App.tsx",
    "src/styles.css",
    "server/domain.mjs",
    "server/db.mjs",
    "server/server.mjs",
    "tests/domain.test.mjs",
    "tests/api.test.mjs",
}

PACKAGE_JSON = {
    "name": "factory-product",
    "version": "1.0.0",
    "private": True,
    "type": "module",
    "scripts": {
        "dev": "vite --host 0.0.0.0",
        "build": "vite build",
        "test": "node --test tests/*.test.mjs",
        "start": "node server/server.mjs",
    },
    "dependencies": {
        "react": "19.1.1",
        "react-dom": "19.1.1",
    },
    "devDependencies": {
        "@types/react": "19.1.10",
        "@types/react-dom": "19.1.7",
        "@vitejs/plugin-react": "5.0.2",
        "typescript": "5.9.2",
        "vite": "7.1.3",
    },
    "engines": {"node": ">=22"},
}

TRUSTED_TEXT_FILES = {
    "tsconfig.json": """{
  \"compilerOptions\": {
    \"target\": \"ES2022\",
    \"useDefineForClassFields\": true,
    \"lib\": [\"ES2022\", \"DOM\", \"DOM.Iterable\"],
    \"allowJs\": false,
    \"skipLibCheck\": true,
    \"esModuleInterop\": true,
    \"allowSyntheticDefaultImports\": true,
    \"strict\": true,
    \"forceConsistentCasingInFileNames\": true,
    \"module\": \"ESNext\",
    \"moduleResolution\": \"Bundler\",
    \"resolveJsonModule\": true,
    \"isolatedModules\": true,
    \"noEmit\": true,
    \"jsx\": \"react-jsx\"
  },
  \"include\": [\"src\"]
}
""",
    "vite.config.js": """import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/api': 'http://127.0.0.1:8080'
    }
  }
});
""",
    ".dockerignore": """node_modules
dist
.git
.factory-work
*.log
""",
    "Dockerfile": """FROM node:22-bookworm-slim AS build
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci --ignore-scripts --no-audit --no-fund
COPY index.html vite.config.js tsconfig.json ./
COPY src ./src
RUN npm run build

FROM node:22-bookworm-slim AS runtime
WORKDIR /app
ENV NODE_ENV=production PORT=8080 DATA_DIR=/data
COPY --from=build /app/dist ./dist
COPY server ./server
VOLUME [\"/data\"]
EXPOSE 8080
CMD [\"node\", \"server/server.mjs\"]
""",
    ".github/workflows/ci.yml": """name: CI
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
      - uses: actions/setup-node@v4
        with:
          node-version: '22'
          cache: npm
      - run: npm ci --ignore-scripts --no-audit --no-fund
      - run: npm test
      - run: npm run build
      - run: docker build -t product-ci .
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
        }
        for item in history[-80:]
    ]
    prompt = f"""
You are the principal product architect for an autonomous software factory.
Propose {count} DIFFERENT, substantial, portfolio-grade software products that solve plausible real-world pain points.
Reject toy apps, calculators, trivial converters, basic CRUD lists, simple note apps, landing pages, thin dashboards, and superficial demos.
Do not claim live research, user interviews, market statistics, or evidence you did not actually obtain.
Avoid malware, credential theft, surveillance, evasion, exploit tooling, weapons, gambling, adult content, and destructive automation.
Prefer serious developer tooling, data-quality, accessibility, education, operations, workflow automation, or small-business products.

Every idea must justify a production-style architecture with:
- React + TypeScript frontend,
- Node.js REST backend,
- SQLite persistence,
- at least 6 connected product features,
- a meaningful domain engine rather than UI-only CRUD,
- validation and error handling,
- persistence and an audit/history concept when relevant,
- import/export, analysis, simulation, comparison, reporting, or another non-trivial workflow,
- deterministic domain tests and API integration tests.

Choose problems that can be fully useful without paid services or fake third-party integrations. The app may run entirely locally/self-hosted.
Do not repeat or lightly reskin anything in PREVIOUS_RELEASES.

PREVIOUS_RELEASES={json.dumps(previous, ensure_ascii=False)}

Return ONLY JSON with this schema:
{{"ideas":[{{"name":"...","slug":"lowercase-kebab-case","target_user":"...","problem":"...","pain":"...","solution":"...","verification":"Deterministic domain and REST API checks covering normal, edge, invalid and persistence cases","category":"...","keywords":["..."],"major_features":["...","...","...","...","...","..."],"technology":"React/TypeScript + Node.js + SQLite"}}]}}
"""
    payload = _request(prompt, temperature=0.68)
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
        fields = [raw.get(k) for k in ("name", "target_user", "problem", "pain", "solution", "verification", "category")]
        if not slug or not all(isinstance(x, str) and x.strip() for x in fields) or len(features) < 6:
            continue
        ideas.append(Idea(
            str(raw["name"]).strip(), slug, str(raw["target_user"]).strip(),
            str(raw["problem"]).strip(), str(raw["pain"]).strip(), str(raw["solution"]).strip(),
            str(raw["verification"]).strip(), str(raw["category"]).strip(), keywords, features,
            "React/TypeScript + Node.js + SQLite",
        ))
    if not ideas:
        raise RuntimeError("Gemini produced no valid production-grade candidate ideas")
    return ideas


def _write_trusted_scaffold(destination: Path, idea: Idea) -> list[str]:
    package = dict(PACKAGE_JSON)
    package["name"] = idea.slug
    (destination / "package.json").write_text(json.dumps(package, indent=2) + "\n", encoding="utf-8")
    for relative, content in TRUSTED_TEXT_FILES.items():
        path = destination / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    (destination / "project.json").write_text(json.dumps(idea.to_dict(), indent=2) + "\n", encoding="utf-8")
    return ["package.json", *TRUSTED_TEXT_FILES.keys(), "project.json"]


def build_project(idea: Idea, destination: Path, repair_feedback: list[str] | None = None) -> list[str]:
    feedback = repair_feedback or []
    prompt = f"""
You are the senior implementation team of an autonomous software factory.
Build one COMPLETE production-style self-hosted product for this selected idea:
{json.dumps(idea.to_dict(), ensure_ascii=False)}

This is a REAL PRODUCT gate. A pretty mockup is a failure.

The factory owns package.json, TypeScript/Vite config, Dockerfile and GitHub CI. You generate ONLY the requested application source files.

Mandatory implementation contract:
- Return ONLY JSON.
- Implement ALL major_features with real logic; do not fake integrations, results, buttons, storage, or success states.
- Frontend: React + TypeScript. App.tsx must provide multiple connected workflows, responsive/accessibility-aware UI, explicit loading/empty/error states and real calls to same-origin `/api/...` endpoints.
- Backend: Node.js ES modules using ONLY built-in Node APIs. Use `node:http` for REST and `node:sqlite` DatabaseSync for persistence. Do not import Express or any undeclared backend package.
- server/domain.mjs must contain substantial pure domain logic that can be tested without HTTP or SQLite.
- server/db.mjs must own schema creation/migrations and parameterized SQLite operations. Never build SQL by concatenating user input.
- server/server.mjs must export `createAppServer({{ dbPath, staticDir }})` without listening during import. When executed directly, listen on process.env.PORT or 8080, use process.env.DATA_DIR or `./data`, expose `/api/health`, implement the product REST API, return JSON errors with sensible 4xx/5xx codes, and serve the Vite `dist` directory for non-API routes.
- API input must be validated server-side. IDs, numbers, dates and structured payloads must reject invalid values rather than coercing dangerous garbage.
- tests/domain.test.mjs must use node:test + node:assert and contain at least 6 meaningful domain assertions covering normal, boundary and invalid cases.
- tests/api.test.mjs must start createAppServer on an ephemeral local port, use a temporary SQLite file, call the REAL REST API, and contain at least 6 meaningful assertions covering health, create/write, read, invalid input, persistence/state transition, and one failure/edge case. Close the server and clean temporary files.
- The test suite must not contact the public internet.
- README must document the real problem, users, architecture, data model, API surface, all implemented capabilities, exact local commands (`npm install`, `npm test`, `npm run build`, `npm start`), Docker usage, privacy/security properties, verification evidence expected from CI, and honest limitations.
- No TODOs, placeholders, lorem ipsum, fake metrics, hard-coded fake success data, claims of tests having passed, or unfinished controls.
- No eval, Function constructor, child_process, shell execution, dynamic code loading, WebSocket, remote analytics, trackers or credentials.
- Frontend network calls must be same-origin `/api/...` only.
- If REPAIR_FEEDBACK is non-empty, correct every item instead of hiding or deleting the affected feature.

REPAIR_FEEDBACK={json.dumps(feedback, ensure_ascii=False)}

Return exactly this schema and no additional file paths:
{{"files":{{
  "README.md":"...",
  "index.html":"...",
  "src/main.tsx":"...",
  "src/App.tsx":"...",
  "src/styles.css":"...",
  "server/domain.mjs":"...",
  "server/db.mjs":"...",
  "server/server.mjs":"...",
  "tests/domain.test.mjs":"...",
  "tests/api.test.mjs":"..."
}}}}
"""
    payload = _request(prompt, temperature=0.22)
    files = payload.get("files")
    if not isinstance(files, dict):
        raise RuntimeError("Gemini build response is missing files")
    if set(files) != GENERATED_FILES:
        raise RuntimeError("Gemini build returned unexpected or missing source paths")

    destination.mkdir(parents=True, exist_ok=True)
    written: list[str] = []
    for relative in sorted(GENERATED_FILES):
        content = files.get(relative)
        if not isinstance(content, str) or len(content.strip()) < 120:
            raise RuntimeError(f"Gemini returned invalid {relative}")
        path = destination / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content.rstrip() + "\n", encoding="utf-8")
        written.append(relative)

    written.extend(_write_trusted_scaffold(destination, idea))
    return sorted(written)
