"""Gemini-backed idea discovery and high-end browser-app generation.

Model output is treated as untrusted data. It never controls shell commands.
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
ALLOWED_FILES = {"README.md", "index.html", "style.css", "core.js", "app.js", "test-core.js"}
RETRYABLE_HTTP_CODES = {429, 500, 502, 503, 504}
MAX_ATTEMPTS_PER_MODEL = max(1, int(os.getenv("GEMINI_RETRY_ATTEMPTS", "2")))


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
        for item in history[-60:]
    ]
    prompt = f"""
You are the senior product-discovery stage of an autonomous software factory.
Propose {count} DIFFERENT, substantial, portfolio-grade software products that solve plausible real-world pain points.
These must NOT be toy apps, one-screen calculators, trivial converters, basic CRUD lists, simple note apps, or superficial demos.
Do not claim live research, user interviews, market statistics, or evidence you did not actually obtain.
Avoid malware, credential theft, surveillance, evasion, exploit tooling, weapons, gambling, adult content, and destructive automation.
Prefer serious developer tooling, data-quality, accessibility, education, operations, personal productivity, or small-business workflows.
Each product must still be feasible as a dependency-free local-first browser app using HTML/CSS/JavaScript only.
A strong idea should include a non-trivial data model, multiple connected workflows, import/export or meaningful persistence, diagnostics/analysis, edge-case handling, and at least one advanced interaction or visualization.
Each idea MUST have at least 5 concrete major features and a deterministic verification plan for its core engine.
Do not repeat or lightly reskin anything in PREVIOUS_RELEASES.

PREVIOUS_RELEASES={json.dumps(previous, ensure_ascii=False)}

Return ONLY JSON with this schema:
{{"ideas":[{{"name":"...","slug":"lowercase-kebab-case","target_user":"...","problem":"...","pain":"...","solution":"...","verification":"Deterministic local checks for the core engine, including at least 3 representative cases","category":"...","keywords":["..."],"major_features":["...","...","...","...","..."],"technology":"HTML/CSS/JavaScript"}}]}}
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
        fields = [raw.get(k) for k in ("name", "target_user", "problem", "pain", "solution", "verification", "category")]
        if not slug or not all(isinstance(x, str) and x.strip() for x in fields) or len(features) < 5:
            continue
        ideas.append(Idea(
            str(raw["name"]).strip(), slug, str(raw["target_user"]).strip(),
            str(raw["problem"]).strip(), str(raw["pain"]).strip(), str(raw["solution"]).strip(),
            str(raw["verification"]).strip(), str(raw["category"]).strip(), keywords, features,
            "HTML/CSS/JavaScript",
        ))
    if not ideas:
        raise RuntimeError("Gemini produced no valid high-end candidate ideas")
    return ideas


def build_project(idea: Idea, destination: Path, repair_feedback: list[str] | None = None) -> list[str]:
    feedback = repair_feedback or []
    prompt = f"""
You are the senior build stage of an autonomous software factory.
Build one COMPLETE, portfolio-grade, local-first browser application for this selected idea:
{json.dumps(idea.to_dict(), ensure_ascii=False)}

This is a REAL PRODUCT gate, not a demo generator.

Mandatory architecture and quality rules:
- Return ONLY JSON.
- Core functionality must work by opening index.html locally in a modern browser.
- Use only HTML, CSS and vanilla JavaScript. No external libraries, CDNs, APIs, analytics, trackers, credentials, or network calls.
- Build a substantial multi-section product, not a toy. Implement ALL major_features, or clearly state a genuine technical limitation and do not fake it.
- core.js must contain the deterministic product engine/data transforms as environment-independent functions and expose them through `globalThis.ProductCore` so both browser and Node can use the same real logic.
- test-core.js must execute the REAL core.js engine under Node and include at least 5 meaningful assertions covering normal cases, edge cases, invalid input, and a multi-step workflow. It must exit non-zero on failure and print a concise success message only after every assertion passes.
- app.js must use ProductCore for core behavior rather than duplicating fake UI-only logic.
- index.html must load core.js before app.js.
- Include meaningful state management, input validation, useful empty/error states, responsive design, keyboard accessibility, and at least one non-trivial workflow such as import/export, analysis, comparison, simulation, visualization, history, or structured editing when relevant.
- Prefer localStorage for durable user-created state when useful; provide a clear reset/export path when state is persisted.
- Do not include placeholders, TODOs, fake buttons, fake integrations, fabricated results, hard-coded success screens, or claims that tests passed.
- Never use eval(), Function(), document.write(), remote fetch(), WebSocket, or dynamic script injection.
- README must include: real problem, intended users, implemented capabilities, exact usage, verification command `node test-core.js`, privacy/local-first behavior, architecture, and honest limitations.
- If REPAIR_FEEDBACK is non-empty, fix every item rather than hiding it.

REPAIR_FEEDBACK={json.dumps(feedback, ensure_ascii=False)}

Schema:
{{"files":{{"README.md":"...","index.html":"...","style.css":"...","core.js":"...","app.js":"...","test-core.js":"..."}}}}
"""
    payload = _request(prompt, temperature=0.28)
    files = payload.get("files")
    if not isinstance(files, dict):
        raise RuntimeError("Gemini build response is missing files")
    if set(files) != ALLOWED_FILES:
        raise RuntimeError("Gemini build returned unexpected file paths")

    destination.mkdir(parents=True, exist_ok=True)
    for name in sorted(ALLOWED_FILES):
        content = files.get(name)
        if not isinstance(content, str) or len(content.strip()) < 80:
            raise RuntimeError(f"Gemini returned invalid {name}")
        (destination / name).write_text(content.rstrip() + "\n", encoding="utf-8")
    (destination / "project.json").write_text(json.dumps(idea.to_dict(), indent=2) + "\n", encoding="utf-8")
    return sorted([*ALLOWED_FILES, "project.json"])
