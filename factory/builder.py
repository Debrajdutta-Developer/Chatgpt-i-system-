"""Trusted, dependency-free product blueprints.

Generated text is data only; validation commands are controlled by validator.py.
"""
from __future__ import annotations
import json
from pathlib import Path
from .models import Idea

PATH_SOURCE = r'''"""Cross-platform repository path portability audit."""
from __future__ import annotations
import argparse, json
from pathlib import Path
RESERVED = {"con", "prn", "aux", "nul", *(f"com{i}" for i in range(1,10)), *(f"lpt{i}" for i in range(1,10))}
def audit(root: Path) -> list[dict]:
    paths = sorted(p.relative_to(root).as_posix() for p in root.rglob("*") if ".git" not in p.relative_to(root).parts)
    findings=[]; folded={}
    for value in paths:
        key=value.casefold(); folded.setdefault(key, []).append(value)
        for part in Path(value).parts:
            stem=part.rstrip(". ").split(".",1)[0].casefold()
            if stem in RESERVED: findings.append({"kind":"reserved-name","path":value,"component":part})
            if part != part.rstrip(". "): findings.append({"kind":"trailing-dot-or-space","path":value,"component":part})
    for values in folded.values():
        if len(values)>1: findings.append({"kind":"case-collision","paths":values})
    return findings
def main(argv=None):
    p=argparse.ArgumentParser(description="Audit paths for cross-platform portability hazards")
    p.add_argument("root", nargs="?", default="."); p.add_argument("--json", action="store_true"); a=p.parse_args(argv)
    root=Path(a.root)
    if not root.is_dir(): p.error(f"not a directory: {root}")
    findings=audit(root)
    if a.json: print(json.dumps(findings, indent=2))
    else:
        for item in findings: print(f"{item['kind']}: {item.get('path', ', '.join(item.get('paths', [])))}")
        print(f"{len(findings)} portability issue(s)")
    return 1 if findings else 0
if __name__ == "__main__": raise SystemExit(main())
'''
CSV_SOURCE = r'''"""Infer and compare lightweight CSV contracts."""
from __future__ import annotations
import argparse, csv, json
from pathlib import Path
def kind(value):
    if not value.strip(): return "empty"
    try: int(value); return "integer"
    except ValueError: pass
    try: float(value); return "number"
    except ValueError: return "text"
def infer(path):
    with path.open(newline="", encoding="utf-8-sig") as f:
        rows=csv.DictReader(f)
        if not rows.fieldnames: raise ValueError("CSV requires a header")
        types={name:set() for name in rows.fieldnames}
        for row in rows:
            if None in row: raise ValueError("row has more fields than the header")
            for name in rows.fieldnames: types[name].add(kind(row.get(name) or ""))
    return {"columns":[{"name":n,"types":sorted(types[n])} for n in rows.fieldnames]}
def compare(actual, expected):
    findings=[]; a={x["name"]:x["types"] for x in actual["columns"]}; e={x["name"]:x["types"] for x in expected["columns"]}
    for name in sorted(e.keys()-a.keys()): findings.append({"kind":"missing-column","column":name})
    for name in sorted(a.keys()-e.keys()): findings.append({"kind":"unexpected-column","column":name})
    for name in sorted(a.keys()&e.keys()):
        if a[name] != e[name]: findings.append({"kind":"type-drift","column":name,"expected":e[name],"actual":a[name]})
    return findings
def main(argv=None):
    p=argparse.ArgumentParser(description="Infer or verify a CSV schema contract"); p.add_argument("csv", type=Path); p.add_argument("--write-contract", type=Path); p.add_argument("--contract", type=Path); a=p.parse_args(argv)
    if bool(a.write_contract)==bool(a.contract): p.error("choose exactly one of --write-contract or --contract")
    try: actual=infer(a.csv)
    except (OSError, ValueError, csv.Error) as exc: p.error(str(exc))
    if a.write_contract: a.write_contract.write_text(json.dumps(actual,indent=2)+"\n"); return 0
    findings=compare(actual,json.loads(a.contract.read_text())); print(json.dumps(findings,indent=2)); return 1 if findings else 0
if __name__ == "__main__": raise SystemExit(main())
'''
TEXT_SOURCE = r'''"""Read-only text hygiene scanner."""
from __future__ import annotations
import argparse, json
from pathlib import Path
def scan(root):
    findings=[]
    for path in sorted(p for p in root.rglob("*") if p.is_file() and ".git" not in p.parts):
        data=path.read_bytes()
        if b"\0" in data: continue
        try: text=data.decode("utf-8")
        except UnicodeDecodeError: continue
        rel=path.relative_to(root).as_posix()
        if b"\r\n" in data and data.replace(b"\r\n",b"").find(b"\n")>=0: findings.append({"kind":"mixed-line-endings","path":rel})
        for number,line in enumerate(text.splitlines(),1):
            if line.endswith((" ","\t")): findings.append({"kind":"trailing-whitespace","path":rel,"line":number})
        if data and not data.endswith((b"\n",b"\r")): findings.append({"kind":"missing-final-newline","path":rel})
    return findings
def main(argv=None):
    p=argparse.ArgumentParser(); p.add_argument("root",nargs="?",default=".",type=Path); p.add_argument("--json",action="store_true"); a=p.parse_args(argv)
    if not a.root.is_dir(): p.error("root must be a directory")
    out=scan(a.root); print(json.dumps(out,indent=2) if a.json else "\n".join(f"{x['kind']}: {x['path']}" for x in out)); return 1 if out else 0
if __name__ == "__main__": raise SystemExit(main())
'''
TESTS = {
"portable-path-auditor": '''from pathlib import Path\nfrom tempfile import TemporaryDirectory\nimport unittest\nfrom src.tool import audit\nclass Tests(unittest.TestCase):\n def test_hazards(self):\n  with TemporaryDirectory() as d:\n   r=Path(d); (r/"Readme").write_text(""); (r/"README").write_text(""); (r/"CON.txt").write_text(""); kinds={x["kind"] for x in audit(r)}\n  self.assertEqual(kinds,{"case-collision","reserved-name"})\n def test_clean(self):\n  with TemporaryDirectory() as d:\n   r=Path(d); (r/"safe.txt").write_text(""); self.assertEqual(audit(r),[])\n''',
"csv-contract-probe": '''from pathlib import Path\nfrom tempfile import TemporaryDirectory\nimport unittest\nfrom src.tool import infer,compare\nclass Tests(unittest.TestCase):\n def test_infer_and_drift(self):\n  with TemporaryDirectory() as d:\n   p=Path(d)/"x.csv"; p.write_text("id,name\\n1,Ada\\n")\n   schema=infer(p); self.assertEqual(compare(schema,schema),[]); p.write_text("id,extra\\nhello,x\\n"); kinds={x["kind"] for x in compare(infer(p),schema)}\n  self.assertEqual(kinds,{"missing-column","unexpected-column","type-drift"})\n''',
"text-hygiene-scanner": '''from pathlib import Path\nfrom tempfile import TemporaryDirectory\nimport unittest\nfrom src.tool import scan\nclass Tests(unittest.TestCase):\n def test_findings_and_binary_skip(self):\n  with TemporaryDirectory() as d:\n   r=Path(d); (r/"bad.txt").write_bytes(b"one  \\r\\ntwo\\nlast"); (r/"bin.dat").write_bytes(b"x\\0y"); kinds={x["kind"] for x in scan(r)}\n  self.assertEqual(kinds,{"mixed-line-endings","trailing-whitespace","missing-final-newline"})\n'''}
SOURCES={"portable-path-auditor":PATH_SOURCE,"csv-contract-probe":CSV_SOURCE,"text-hygiene-scanner":TEXT_SOURCE}

def build(idea: Idea, destination: Path) -> list[str]:
    if idea.slug not in SOURCES: raise ValueError(f"no trusted blueprint for {idea.slug}")
    (destination/"src").mkdir(parents=True); (destination/"tests").mkdir()
    (destination/"src"/"__init__.py").write_text(""); (destination/"src"/"tool.py").write_text(SOURCES[idea.slug])
    (destination/"tests"/"test_tool.py").write_text(TESTS[idea.slug])
    (destination/"README.md").write_text(f"# {idea.name}\n\n{idea.problem}\n\n## Who it helps\n\n{idea.target_user.capitalize()}. {idea.pain}\n\n## What it does\n\n{idea.solution}\n\n## Run\n\nRequires Python 3.10+ and has no third-party dependencies.\n\n```bash\npython -m src.tool --help\n```\n\nThe CLI is read-only except where an explicit contract-writing option is documented by `--help`. Exit code 0 means clean/matching, 1 means findings, and 2 means invalid input.\n\n## Test\n\n```bash\npython -m unittest discover -s tests -v\npython -m compileall -q src tests\n```\n\n## Limitations\n\nThe tool intentionally performs local deterministic analysis and does not inspect remote services.\n")
    (destination/"project.json").write_text(json.dumps(idea.to_dict(),indent=2)+"\n")
    return [str(p.relative_to(destination)) for p in destination.rglob("*") if p.is_file()]
