"""One-cycle factory orchestrator."""
from __future__ import annotations
import argparse, json, shutil, sys
from datetime import datetime, timezone
from pathlib import Path
from .builder import build
from .config import Config
from .evaluator import evaluate
from .history import load_history, write_history
from .models import CycleResult
from .planner import discover, select
from .publisher import promote, remote_status
from .utils import FactoryLocked, Lock
from .validator import validate

def atomic_json(path: Path, value: object) -> None:
    temporary=path.with_name(f".{path.name}.tmp")
    temporary.write_text(json.dumps(value,indent=2,sort_keys=True)+"\n",encoding="utf-8"); temporary.replace(path)

def recent_ideas(reports: Path, limit: int = 20) -> list[dict]:
    """Return selected ideas from recent valid reports for duplicate screening."""
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

def run_cycle(config: Config, dry_run: bool=False, now: datetime|None=None) -> CycleResult:
    now=now or datetime.now(timezone.utc); stamp=now.strftime("%Y%m%dT%H%M%SZ"); cycle=f"cycle-{stamp}"
    config.reports.mkdir(parents=True,exist_ok=True); config.projects.mkdir(parents=True,exist_ok=True)
    with Lock(config.lock):
        history=load_history(config.history); candidates=discover(); existing={p.name for p in config.projects.iterdir() if p.is_dir()}
        duplicate_evidence = [*history, *recent_ideas(config.reports)]
        idea,rejected=select(candidates,duplicate_evidence,existing)
        report={"cycle_id":cycle,"timestamp":now.isoformat(),"dry_run":dry_run,"candidate_ideas":[x.to_dict() for x in candidates],"rejected_candidate_ideas":rejected,"selected_idea":idea.to_dict() if idea else None,"validation_results":[],"repair_attempts":[],"quality_scores":{},"final_score":0,"release_threshold":config.release_threshold,"files_created":[],"commit":None,"remote_status":"NOT_ATTEMPTED","known_limitations":["Idea discovery is limited to the reviewed local blueprint catalog."]}
        suffix=idea.slug if idea else "no-release"; report_path=config.reports/f"{now.date().isoformat()}-{stamp[9:15].lower()}-{suffix}.json"
        if idea is None:
            report.update(final_status="NO_RELEASE",reason="all reviewed blueprints duplicate existing or historical projects")
            atomic_json(report_path,report); return CycleResult("NO_RELEASE",str(report_path),message=report["reason"])
        if dry_run:
            report.update(final_status="NO_RELEASE",reason="dry-run selected and planned an idea; generation and publication intentionally skipped")
            atomic_json(report_path,report); return CycleResult("NO_RELEASE",str(report_path),message=report["reason"])
        staging=config.work/cycle/idea.slug; config.work.mkdir(parents=True,exist_ok=True)
        final_results=[]; files=[]
        try:
            for attempt in range(config.max_repair_attempts+1):
                shutil.rmtree(staging,ignore_errors=True); staging.mkdir(parents=True)
                files=build(idea,staging); final_results=validate(staging)
                if all(x.exit_code==0 for x in final_results): break
                report["repair_attempts"].append({"attempt":attempt+1,"action":"recreated project from reviewed blueprint","failure_classes":[x.failure_class for x in final_results if x.failure_class]})
            scores,total,releasable=evaluate(staging,idea,final_results,config.release_threshold)
            report.update(validation_results=[x.to_dict() for x in final_results],quality_scores=scores,final_score=total,files_created=files)
            if not releasable:
                failure_class = next((x.failure_class for x in final_results if x.critical and x.failure_class), "QUALITY_FAILURE")
                report.update(final_status="FAILED",failure_class=failure_class,reason="critical validation or quality threshold failed",remote_status="NOT_ATTEMPTED")
                atomic_json(report_path,report); return CycleResult("FAILED",str(report_path),message=report["reason"])
            target=config.projects/idea.slug; promote(staging,target)
            publication=remote_status(config.root)
            entry={"cycle":cycle,"name":idea.name,"slug":idea.slug,"date":str(now.date()),"problem":idea.problem,"purpose":idea.solution,"category":idea.category,"keywords":list(idea.keywords),"major_features":list(idea.major_features),"technology":idea.technology,"validation_summary":[{"command":x.command,"status":x.status} for x in final_results],"quality_score":total,"report_path":str(report_path.relative_to(config.root)),"commit_hash":None}
            write_history(config.history,[*history,entry])
            report.update(final_status="SUCCESS",project_name=idea.name,slug=idea.slug,problem=idea.problem,target_user=idea.target_user,technology=idea.technology,project_path=str(target.relative_to(config.root)),remote_status=publication,reason="project passed critical validation and quality gate")
            atomic_json(report_path,report)
            return CycleResult("SUCCESS",str(report_path),str(target),report["reason"])
        finally:
            shutil.rmtree(config.work/cycle,ignore_errors=True)
            if config.work.exists() and not any(config.work.iterdir()): config.work.rmdir()

def main(argv=None) -> int:
    parser=argparse.ArgumentParser(description="Run exactly one autonomous software factory cycle")
    parser.add_argument("--dry-run",action="store_true",help="discover and select, but never generate, commit, or publish")
    args=parser.parse_args(argv)
    try: result=run_cycle(Config(),args.dry_run)
    except FactoryLocked as exc: print(json.dumps({"status":"FAILED","failure_class":"ENVIRONMENT_FAILURE","message":str(exc)})); return 2
    except Exception as exc: print(json.dumps({"status":"FAILED","failure_class":"ENVIRONMENT_FAILURE","message":str(exc)})); return 2
    print(json.dumps({"status":result.status,"report":result.report_path,"project":result.project_path,"message":result.message}))
    return 0 if result.status in {"SUCCESS","NO_RELEASE"} else 1
if __name__=="__main__": raise SystemExit(main())
