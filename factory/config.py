"""Factory configuration with repository-relative defaults."""
from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

@dataclass(frozen=True)
class Config:
    root: Path = ROOT
    release_threshold: int = 80
    max_repair_attempts: int = 3

    @property
    def projects(self) -> Path: return self.root / "projects"
    @property
    def reports(self) -> Path: return self.root / "reports"
    @property
    def history(self) -> Path: return self.root / "factory-history.json"
    @property
    def work(self) -> Path: return self.root / ".factory-work"
    @property
    def lock(self) -> Path: return self.root / ".factory.lock"
