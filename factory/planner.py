"""Deterministic idea discovery and duplicate-aware selection."""
from __future__ import annotations
from .history import is_duplicate
from .models import Idea

def discover() -> list[Idea]:
    return [
      Idea("Portable Path Auditor", "portable-path-auditor", "cross-platform development teams", "Repository paths can collide on case-insensitive systems or use Windows-reserved names.", "These defects often remain invisible until checkout on another operating system.", "A local CLI that audits real paths for portability hazards without modifying them.", "Fixture repositories exercise collisions, reserved names, and clean trees.", "developer tooling", ("filesystem", "portability", "paths", "git"), ("case-fold collision detection", "reserved-name detection", "JSON output")),
      Idea("CSV Contract Probe", "csv-contract-probe", "data workers and small teams", "Incoming CSV files silently change columns or primitive value shapes.", "Downstream scripts fail late or ingest malformed data.", "A local CLI that infers a compact schema and compares files against a saved contract.", "Automated fixtures cover schema creation, matching input, and drift.", "data quality", ("csv", "schema", "drift", "validation"), ("schema inference", "contract comparison", "machine-readable findings")),
      Idea("Text Hygiene Scanner", "text-hygiene-scanner", "developers maintaining cross-platform repositories", "Mixed line endings and trailing whitespace cause noisy diffs and tooling failures.", "The defects are hard to spot visually and often spread through edits.", "A read-only CLI that locates exact files and lines with text hygiene defects.", "Fixtures cover CRLF mixing, trailing whitespace, binary exclusion, and clean files.", "developer tooling", ("text", "line-endings", "whitespace", "repository"), ("mixed-ending detection", "trailing-whitespace locations", "binary exclusion")),
    ]

def select(candidates: list[Idea], history: list[dict], existing_slugs: set[str]) -> tuple[Idea | None, list[dict]]:
    rejected: list[dict] = []
    for idea in candidates:
        duplicate, score, matched = is_duplicate(idea, history)
        if idea.slug in existing_slugs: duplicate, score, matched = True, 1.0, idea.slug
        if duplicate:
            rejected.append({"idea": idea.to_dict(), "reason": "duplicate", "similarity": round(score, 3), "matched": matched})
            continue
        return idea, rejected
    return None, rejected
