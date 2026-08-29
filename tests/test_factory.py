from datetime import datetime, timezone
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from factory.config import Config
from factory.builder import build
from factory.evaluator import evaluate
from factory.history import is_duplicate, load_history, write_history
from factory.models import Idea, ValidationResult
from factory.orchestrator import recent_ideas, run_cycle
from factory.planner import discover, select
from factory.provider import _files_map, _idea_items
from factory.utils import FactoryLocked, Lock
from factory.validator import classify
from factory.validator import validate

class FactoryTests(unittest.TestCase):
 def test_history_atomic_round_trip_and_duplicate(self):
  with TemporaryDirectory() as d:
   p=Path(d)/"history.json"; write_history(p,[{"name":"Portable Path Auditor","problem":"case path collision","keywords":["filesystem"]}]); history=load_history(p)
   duplicate,score,_=is_duplicate(discover()[0],history)
  self.assertTrue(duplicate); self.assertGreater(score,0)
 def test_selection_rejects_existing_slug(self):
  selected,rejected=select(discover(),[],{"portable-path-auditor"})
  self.assertEqual(selected.slug,"csv-contract-probe"); self.assertEqual(rejected[0]["reason"],"duplicate")
 def test_failure_classification(self):
  self.assertEqual(classify("No module named x",1,"test"),"DEPENDENCY_FAILURE")
  self.assertEqual(classify("assertion failed",1,"test"),"TEST_FAILURE"); self.assertIsNone(classify("",0,"test"))
 def test_quality_gate_cannot_be_overridden_by_score(self):
  idea=discover()[0]
  with TemporaryDirectory() as d:
   p=Path(d); (p/"src").mkdir(); (p/"README.md").write_text("x"*600)
   scores,total,release=evaluate(p,idea,[ValidationResult(["python","-m","unittest"],1,"FAIL","TEST_FAILURE","bad")],80)
  self.assertGreaterEqual(total,60); self.assertFalse(release); self.assertEqual(scores["correctness"],0)
 def test_dry_run_reports_no_release_and_isolated(self):
  with TemporaryDirectory() as d:
   root=Path(d); config=Config(root=root); write_history(config.history,[])
   result=run_cycle(config,True,datetime(2026,8,29,3,0,tzinfo=timezone.utc)); report=json.loads(Path(result.report_path).read_text())
   self.assertEqual(result.status,"NO_RELEASE"); self.assertFalse(any(config.projects.iterdir())); self.assertEqual(report["final_status"],"NO_RELEASE")
 def test_success_promotes_only_after_validation(self):
  with TemporaryDirectory() as d:
   root=Path(d); config=Config(root=root); write_history(config.history,[])
   result=run_cycle(config,False,datetime(2026,8,29,3,1,tzinfo=timezone.utc)); report=json.loads(Path(result.report_path).read_text())
   self.assertEqual(result.status,"SUCCESS"); self.assertTrue((root/"projects"/"portable-path-auditor"/"README.md").is_file()); self.assertTrue(all(x["status"]=="PASS" for x in report["validation_results"])); self.assertFalse(config.work.exists())
 def test_every_reviewed_blueprint_builds_and_passes(self):
  for idea in discover():
   with self.subTest(slug=idea.slug), TemporaryDirectory() as d:
    project=Path(d)/idea.slug; project.mkdir(); build(idea,project)
    self.assertTrue(all(result.status=="PASS" for result in validate(project)))
 def test_no_release_when_all_candidates_exist(self):
  with TemporaryDirectory() as d:
   root=Path(d); config=Config(root=root); write_history(config.history,[])
   for idea in discover(): (config.projects/idea.slug).mkdir(parents=True)
   result=run_cycle(config,False,datetime(2026,8,29,3,2,tzinfo=timezone.utc))
  self.assertEqual(result.status,"NO_RELEASE")
 def test_recent_report_ideas_are_loaded_and_bad_json_is_ignored(self):
  with TemporaryDirectory() as d:
   reports=Path(d); (reports/"new.json").write_text(json.dumps({"final_status":"SUCCESS","selected_idea":{"name":"Seen"}})); (reports/"bad.json").write_text("{")
   self.assertEqual(recent_ideas(reports),[{"name":"Seen"}])
 def test_gemini_idea_payload_accepts_object_and_top_level_list(self):
  ideas=[{"name":"Mini DB"}]
  self.assertEqual(_idea_items({"ideas":ideas}),ideas)
  self.assertEqual(_idea_items(ideas),ideas)
  with self.assertRaises(RuntimeError): _idea_items("bad")
 def test_gemini_build_payload_accepts_single_wrapped_list(self):
  files={"README.md":"x"}
  self.assertEqual(_files_map({"files":files}),files)
  self.assertEqual(_files_map([{"files":files}]),files)
  with self.assertRaises(RuntimeError): _files_map([])
 def test_lock_prevents_concurrent_cycle(self):
  with TemporaryDirectory() as d:
   path=Path(d)/"lock"
   with Lock(path):
    with self.assertRaises(FactoryLocked): Lock(path).__enter__()

if __name__=="__main__": unittest.main()
