"""Atomic history access and semantic duplicate detection."""
from __future__ import annotations
import json, os, re, tempfile
from pathlib import Path
from typing import Any
from .models import Idea

WORDS = re.compile(r"[a-z0-9]+")
STOP = {"a", "an", "and", "for", "in", "of", "the", "to", "with"}

def load_history(path: Path) -> list[dict[str, Any]]:
    if not path.exists(): return []
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list): raise ValueError("factory history must be a JSON array")
    return data

def write_history(path: Path, entries: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent, text=True)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(entries, stream, indent=2, sort_keys=True); stream.write("\n"); stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException:
        try: os.unlink(temporary)
        except FileNotFoundError: pass
        raise

def tokens(value: object) -> set[str]:
    if isinstance(value, list): value = " ".join(map(str, value))
    return {word for word in WORDS.findall(str(value).lower()) if word not in STOP}

def similarity(idea: Idea, entry: dict[str, Any]) -> float:
    current = tokens(" ".join((idea.name, idea.problem, idea.solution, idea.category, *idea.keywords, *idea.major_features)))
    previous = tokens(" ".join(str(entry.get(k, "")) for k in ("name", "problem", "solution", "purpose", "category", "keywords", "major_features")))
    return len(current & previous) / len(current | previous) if current | previous else 0.0

def is_duplicate(idea: Idea, history: list[dict[str, Any]], threshold: float = 0.38) -> tuple[bool, float, str | None]:
    best = (0.0, None)
    for entry in history:
        score = similarity(idea, entry)
        if score > best[0]: best = (score, str(entry.get("name", "unknown")))
        if idea.slug == entry.get("slug") or idea.name.casefold() == str(entry.get("name", "")).casefold(): return True, 1.0, str(entry.get("name"))
    return best[0] >= threshold, best[0], best[1]
