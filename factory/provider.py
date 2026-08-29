"""Gemini-backed idea discovery and static web-app generation.

Model output is treated as untrusted data. It never controls shell commands.
"""
from __future__ import annotations

import json
import os
import re
import urllib.error
import urllib.request
from pathlib import Path

from .models import Idea

MODEL = os.getenv("GEMINI_MODEL", "gemini-3.7-flash")
API_KEY = os.getenv("GEMINI_API_KEY", "").strip()
API_BASE = "https://generativelanguage.googleapis.com/v1beta/models"
ALLOWED_FILES = {"README.md", "index.html", "style.css", "app.js"}


def available() -> bool:
    return bool(API_KEY)


def _request(prompt: str, temperature: float = 0.5) -> dict:
    if not API_KEY:
        raise RuntimeError("GEMINI_API_KEY is not configured")
    url = f"{API_BASE}/{MODEL}:generateContent"
    body = json.dumps({
        "contents": [{"role": "user", "parts": [{"text": prompt}]}],
        "generationConfig": {
            "temperature": temperature,
            "responseMimeType": "application/json",
        },
    }).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json", "x-goog-api-key": API_KEY},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")[:500]
        raise RuntimeError(f"Gemini HTTP {exc.code}: {detail}") from exc
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(f"Gemini network failure: {exc}") from exc

    try:
        text = payload["candidates"][0]["content"]["parts"][0]["text"]
        return json.loads(text)
    except (KeyError, IndexError, TypeError, json.JSONDecodeError) as exc:
        raise RuntimeError("Gemini returned invalid structured output") from exc


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
You are the discovery stage of an autonomous software factory.
Propose {count} DIFFERENT small, useful, benign software products that solve plausible real-world pain points.
Do not claim live research, user interviews, market statistics, or evidence you did not actually obtain.
Avoid malware, credential theft, surveillance, evasion, exploit tooling, weapons, gambling, adult content, and destructive automation.
Prefer local-first productivity, education, developer, accessibility, data-quality, personal-organization, or small-business utilities.
Each idea must be feasible as a dependency-free browser app using HTML/CSS/JavaScript only.
Do not repeat or lightly reskin anything in PREVIOUS_RELEASES.

PREVIOUS_RELEASES={json.dumps(previous, ensure_ascii=False)}

Return ONLY JSON with this schema:
{{"ideas":[{{"name":"...","slug":"lowercase-kebab-case","target_user":"...","problem":"...","pain":"...","solution":"...","verification":"How the factory can verify core behavior locally","category":"...","keywords":["..."],"major_features":["...","...","..."],"technology":"HTML/CSS/JavaScript"}}]}}
"""
    payload = _request(prompt, temperature=0.75)
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
        if not slug or not all(isinstance(x, str) and x.strip() for x in fields) or len(features) < 3:
            continue
        ideas.append(Idea(
            str(raw["name"]).strip(), slug, str(raw["target_user"]).strip(),
            str(raw["problem"]).strip(), str(raw["pain"]).strip(), str(raw["solution"]).strip(),
            str(raw["verification"]).strip(), str(raw["category"]).strip(), keywords, features,
            "HTML/CSS/JavaScript",
        ))
    if not ideas:
        raise RuntimeError("Gemini produced no valid candidate ideas")
    return ideas


def build_project(idea: Idea, destination: Path, repair_feedback: list[str] | None = None) -> list[str]:
    feedback = repair_feedback or []
    prompt = f"""
You are the build stage of an autonomous software factory.
Build one COMPLETE local-first browser application for this selected idea:
{json.dumps(idea.to_dict(), ensure_ascii=False)}

Rules:
- Return ONLY JSON.
- Core functionality must work by opening index.html locally in a modern browser.
- Use only HTML, CSS and vanilla JavaScript. No external libraries, CDNs, APIs, analytics, trackers, or credentials.
- Do not include placeholders, TODOs, fake buttons, fake integrations, fake results, or claims that tests passed.
- Provide accessible labels, keyboard-usable controls, responsive layout, error handling, empty states, and persistent localStorage only when genuinely useful.
- Never use eval(), Function(), document.write(), remote fetch(), WebSocket, or dynamic script injection.
- README must explain the problem, features, use, privacy/local-first behavior, and limitations.
- If REPAIR_FEEDBACK is non-empty, fix every item.

REPAIR_FEEDBACK={json.dumps(feedback, ensure_ascii=False)}

Schema:
{{"files":{{"README.md":"...","index.html":"...","style.css":"...","app.js":"..."}}}}
"""
    payload = _request(prompt, temperature=0.35)
    files = payload.get("files")
    if not isinstance(files, dict):
        raise RuntimeError("Gemini build response is missing files")
    if set(files) != ALLOWED_FILES:
        raise RuntimeError("Gemini build returned unexpected file paths")

    destination.mkdir(parents=True, exist_ok=True)
    for name in sorted(ALLOWED_FILES):
        content = files.get(name)
        if not isinstance(content, str) or len(content.strip()) < 40:
            raise RuntimeError(f"Gemini returned invalid {name}")
        (destination / name).write_text(content.rstrip() + "\n", encoding="utf-8")
    (destination / "project.json").write_text(json.dumps(idea.to_dict(), indent=2) + "\n", encoding="utf-8")
    return sorted([*ALLOWED_FILES, "project.json"])
