"""Typed lifecycle records shared across factory stages."""
from __future__ import annotations
from dataclasses import asdict, dataclass, field
from typing import Any

@dataclass(frozen=True)
class Idea:
    name: str
    slug: str
    target_user: str
    problem: str
    pain: str
    solution: str
    verification: str
    category: str
    keywords: tuple[str, ...]
    major_features: tuple[str, ...]
    technology: str = "Python standard library"

    def to_dict(self) -> dict[str, Any]:
        data = asdict(self); data["keywords"] = list(self.keywords); data["major_features"] = list(self.major_features); return data

@dataclass
class ValidationResult:
    command: list[str]
    exit_code: int
    status: str
    failure_class: str | None
    summary: str
    critical: bool = True

    def to_dict(self) -> dict[str, Any]: return asdict(self)

@dataclass
class CycleResult:
    status: str
    report_path: str
    project_path: str | None = None
    message: str = ""
