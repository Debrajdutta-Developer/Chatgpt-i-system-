"""Publication status detection; never claims an unauthenticated push."""
from __future__ import annotations
import os, shutil, subprocess
from pathlib import Path

def remote_status(root: Path) -> str:
    if not os.environ.get("GITHUB_ACTIONS"): return "LOCAL_COMPLETE_REMOTE_NOT_PUBLISHED"
    remote=subprocess.run(["git","remote","get-url","origin"],cwd=root,capture_output=True,text=True)
    if remote.returncode: return "LOCAL_COMPLETE_REMOTE_NOT_PUBLISHED"
    return "READY_FOR_WORKFLOW_COMMIT"

def promote(staging: Path, target: Path) -> None:
    if target.exists(): raise FileExistsError(f"project already exists: {target}")
    target.parent.mkdir(parents=True,exist_ok=True); shutil.move(str(staging),str(target))
