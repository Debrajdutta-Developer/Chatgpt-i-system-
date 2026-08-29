"""One-cycle factory orchestrator."""
from __future__ import annotations
import argparse, json, shutil
from datetime import datetime, timezone
from pathlib import Path

from .builder import SOURCES, build
from .config import Config
from .evaluator import evaluate
from .history import load_history, write_history
from .models import CycleResult
from .planner import discover, select
from .provider import available as provider_available, build_project as build_ai_project
from .publisher import promote, remote_status
from .utils import FactoryLocked, Lock
from .validator import validate


def atomic_json(path: Path, value: object) -> None:
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def recent_ideas(reports: Path, limit: int = 20) -> list[dict]:
    ideas = []
    for path in sorted(reports.glob("*.json"), reverse=True)[:limit]:
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
            selected = payload.get("selected_idea") if payload.get("final_status") == "SUCCESS" else None
        except (OSError, json.JSONDecodeError, AttributeError):
            continue
        if isinstance(selected, dict):
            ideas.append(selected)
    return ideas


def _build(idea, staging: Path, feedback: list[str] | None = None) -> list[str]:
    if idea.slug in SOURCES:
        return build(idea, staging)
    if not provider_available():
        raise RuntimeError("AI-selected project requires GEMINI_API_KEY")
    return build_ai_project(idea, staging, feedback)


def run_cycle(config: Config, dry_run: bool = False, now: datetime | None = None) -> CycleResult:
    now = now or datetime.now(timezone.utc)
    stamp = now.strftime("%Y%m%dT%H%M%SZ")
    cycle = f"cycle-{stamp}"
    config.reports.mkdir(parents=True, exist_ok=True)
    config.projects.mkdir(parents=True, exist_ok=True)

    with Lock(config.lock):
        history = load_history(config.history)
        duplicate_evidence = [*history, *recent_ideas(config.reports)]
        candidates = discover(duplicate_evidence, use_provider=True)
        existing = {p.name for p in config.projects.iterdir() if p.is_dir()}
        idea, rejected = select(candidates, duplicate_evidence, existing)
        using_ai = bool(provider_available() and idea and idea.slug not in SOURCES)

        report = {
            "cycle_id": cycle,
            "timestamp": now.isoformat(),
            "dry_run": dry_run,
            "discovery_mode": "gemini" if provider_available() else "reviewed-fallback",
            "validation_profile": "hard-systems-v1" if using_ai else "reviewed-blueprint",
            "candidate_ideas": [x.to_dict() for x in candidates],
            "rejected_candidate_ideas": rejected,
            "selected_idea": idea.to_dict() if idea else None,
            "validation_results": [],
            "repair_attempts": [],
            "quality_scores": {},
            "final_score": 0,
            "release_threshold": config.release_threshold,
            "files_created": [],
            "commit": None,
            "remote_status": "NOT_ATTEMPTED",
            "standalone_status": "PENDING" if using_ai else "NOT_APPLICABLE",
            "end_to_end_status": "PENDING" if using_ai else "NOT_APPLICABLE",
            "known_limitations": [
                "Gemini discovery reasons from model knowledge; the factory does not claim live market research or verified market demand.",
                "A passing systems release proves the generated source compiled or byte-compiled and its factory-required unit/integration suites passed; it does not by itself prove production-scale reliability, security certification, or benchmark superiority.",
                "Generated systems projects intentionally use standard-library-only Java 21, C11, or Python 3.12 profiles so validation can be deterministic and does not depend on unverifiable third-party packages.",
                "Hard projects are scoped implementations of real mechanisms, not claims of complete compatibility with Redis, full SQL standards, industrial compilers, or frontier machine-learning systems unless the tests and documentation explicitly demonstrate that scope.",
            ],
        }
        suffix = idea.slug if idea else "no-release"
        report_path = config.reports / f"{now.date().isoformat()}-{stamp[9:15].lower()}-{suffix}.json"

        if idea is None:
            report.update(final_status="NO_RELEASE", end_to_end_status="NO_RELEASE", reason="all candidates duplicate existing or historical projects")
            atomic_json(report_path, report)
            return CycleResult("NO_RELEASE", str(report_path), message=report["reason"])

        if dry_run:
            report.update(final_status="NO_RELEASE", end_to_end_status="NO_RELEASE", reason="dry-run selected and planned an idea; generation and publication intentionally skipped")
            atomic_json(report_path, report)
            return CycleResult("NO_RELEASE", str(report_path), message=report["reason"])

        staging = config.work / cycle / idea.slug
        config.work.mkdir(parents=True, exist_ok=True)
        final_results = []
        files = []
        feedback: list[str] = []
        try:
            for attempt in range(config.max_repair_attempts + 1):
                shutil.rmtree(staging, ignore_errors=True)
                staging.mkdir(parents=True)
                files = _build(idea, staging, feedback)
                final_results = validate(staging)
                failures = [r.summary or r.failure_class or "validation failure" for r in final_results if r.exit_code != 0]
                if not failures:
                    break
                report["repair_attempts"].append({
                    "attempt": attempt + 1,
                    "action": "regenerated systems project with bounded validation feedback" if using_ai else "recreated project from reviewed blueprint",
                    "failure_classes": [x.failure_class for x in final_results if x.failure_class],
                    "summaries": failures[:8],
                })
                feedback = failures[:8]

            scores, total, releasable = evaluate(staging, idea, final_results, config.release_threshold)
            report.update(
                validation_results=[x.to_dict() for x in final_results],
                quality_scores=scores,
                final_score=total,
                files_created=files,
            )
            if not releasable:
                failure_class = next((x.failure_class for x in final_results if x.critical and x.failure_class), "QUALITY_FAILURE")
                report.update(
                    final_status="FAILED",
                    end_to_end_status="FAILED",
                    standalone_status="NOT_ATTEMPTED",
                    failure_class=failure_class,
                    reason="critical systems validation or quality threshold failed",
                    remote_status="NOT_ATTEMPTED",
                )
                atomic_json(report_path, report)
                return CycleResult("FAILED", str(report_path), message=report["reason"])

            target = config.projects / idea.slug
            promote(staging, target)
            publication = remote_status(config.root)
            entry = {
                "cycle": cycle,
                "name": idea.name,
                "slug": idea.slug,
                "date": str(now.date()),
                "problem": idea.problem,
                "purpose": idea.solution,
                "category": idea.category,
                "keywords": list(idea.keywords),
                "major_features": list(idea.major_features),
                "technology": idea.technology,
                "validation_profile": report["validation_profile"],
                "validation_summary": [{"command": x.command, "status": x.status} for x in final_results],
                "quality_score": total,
                "report_path": str(report_path.relative_to(config.root)),
                "commit_hash": None,
            }
            write_history(config.history, [*history, entry])
            report.update(
                final_status="SUCCESS",
                project_name=idea.name,
                slug=idea.slug,
                problem=idea.problem,
                target_user=idea.target_user,
                technology=idea.technology,
                project_path=str(target.relative_to(config.root)),
                remote_status=publication,
                standalone_status="PENDING" if using_ai else "NOT_APPLICABLE",
                end_to_end_status="PENDING" if using_ai else "SUCCESS",
                reason="project passed critical systems compilation/testing and the quality gate",
            )
            atomic_json(report_path, report)
            return CycleResult("SUCCESS", str(report_path), str(target), report["reason"])
        finally:
            shutil.rmtree(config.work / cycle, ignore_errors=True)
            if config.work.exists() and not any(config.work.iterdir()):
                config.work.rmdir()


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Run exactly one autonomous software factory cycle")
    parser.add_argument("--dry-run", action="store_true", help="discover and select, but never generate, commit, or publish")
    args = parser.parse_args(argv)
    try:
        result = run_cycle(Config(), args.dry_run)
    except FactoryLocked as exc:
        print(json.dumps({"status": "FAILED", "failure_class": "ENVIRONMENT_FAILURE", "message": str(exc)}))
        return 2
    except Exception as exc:
        print(json.dumps({"status": "FAILED", "failure_class": "ENVIRONMENT_FAILURE", "message": str(exc)}))
        return 2
    print(json.dumps({"status": result.status, "report": result.report_path, "project": result.project_path, "message": result.message}))
    return 0 if result.status in {"SUCCESS", "NO_RELEASE"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
