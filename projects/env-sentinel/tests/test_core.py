from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from env_sentinel.cli import main
from env_sentinel.core import audit, parse_dotenv


class EnvSentinelTests(unittest.TestCase):
    def test_parse_dotenv_reports_bad_and_duplicate_entries(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / ".env.example"
            path.write_text("# comment\nexport GOOD=1\nGOOD=2\nbad-key=x\nBROKEN\n", encoding="utf-8")
            keys, findings = parse_dotenv(path)
        self.assertEqual(keys, {"GOOD"})
        self.assertEqual([item.kind for item in findings], ["duplicate", "invalid-key", "invalid-line"])

    def test_audit_compares_source_contract_and_environment(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            contract = root / ".env.example"
            environment = root / ".env"
            contract.write_text("API_URL=\nOLD_FLAG=\n", encoding="utf-8")
            environment.write_text("API_URL=https://local\nEXTRA=yes\n", encoding="utf-8")
            (root / "app.py").write_text('import os\nos.getenv("API_URL")\nos.environ["SECRET"]\n', encoding="utf-8")
            report = audit(root, contract, environment)
        self.assertEqual(report.referenced, ["API_URL", "SECRET"])
        self.assertEqual(
            {(item.kind, item.key) for item in report.findings},
            {("undocumented", "SECRET"), ("unused", "OLD_FLAG"), ("missing", "OLD_FLAG"), ("unexpected", "EXTRA")},
        )

    def test_javascript_and_rust_reference_forms(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            contract = root / ".env.example"
            contract.write_text("TOKEN=\nPORT=\n", encoding="utf-8")
            (root / "app.js").write_text("process.env.TOKEN; process.env['PORT'];", encoding="utf-8")
            (root / "ignored.txt").write_text('process.env.NOT_CODE', encoding="utf-8")
            report = audit(root, contract)
        self.assertEqual(report.referenced, ["PORT", "TOKEN"])
        self.assertEqual(report.findings, [])

    def test_cli_returns_two_for_missing_contract(self):
        with TemporaryDirectory() as directory:
            self.assertEqual(main(["--root", directory]), 2)


if __name__ == "__main__":
    unittest.main()
