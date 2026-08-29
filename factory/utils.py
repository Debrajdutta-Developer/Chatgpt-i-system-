"""Locking and filesystem helpers."""
from __future__ import annotations
import os
from pathlib import Path

class FactoryLocked(RuntimeError): pass
class Lock:
    def __init__(self, path: Path): self.path = path; self.fd: int | None = None
    def __enter__(self):
        try: self.fd = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        except FileExistsError as exc: raise FactoryLocked(f"factory lock already exists: {self.path}") from exc
        os.write(self.fd, str(os.getpid()).encode()); return self
    def __exit__(self, *_):
        if self.fd is not None: os.close(self.fd)
        self.path.unlink(missing_ok=True)
