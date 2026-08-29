from __future__ import annotations

import json
from datetime import datetime, timezone

from .config import Config
from .orchestrator import main


def _report_names(config: Config) -> set[str]:
    if not config.reports.exists():
        return set()
    return {path.name for path in config.reports.glob("*.json") if path.is_file()}


def _write_fallback_failure_report(config: Config) -> str:
    config.reports.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc)
    stamp = now.strftime("%Y%m%dT%H%M%SZ")
    path = config.reports / f"{now.date().isoformat()}-{stamp[9:15].lower()}-environment-failure.json"
    payload = {
        "cycle_id": f"cycle-{stamp}",
        "timestamp": now.isoformat(),
        "final_status": "FAILED",
        "end_to_end_status": "FAILED",
        "failure_class": "ENVIRONMENT_FAILURE",
        "reason": "factory terminated before the orchestrator could create its normal cycle report; inspect the preceding workflow log for the concrete exception",
        "remote_status": "NOT_ATTEMPTED",
        "standalone_status": "NOT_ATTEMPTED",
        "validation_results": [],
        "repair_attempts": [],
        "quality_scores": {},
        "final_score": 0,
    }
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return str(path)


def entrypoint() -> int:
    config = Config()
    before = _report_names(config)
    code = main()
    after = _report_names(config)
    if code != 0 and not (after - before):
        path = _write_fallback_failure_report(config)
        print(json.dumps({"status": "FAILED", "report": path, "message": "fallback failure report created"}))
    return code


raise SystemExit(entrypoint())
