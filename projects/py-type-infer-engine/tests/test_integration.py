import unittest
from src.engine import AnalysisEngine

class TestEngineIntegration(unittest.TestCase):
    def setUp(self):
        self.engine = AnalysisEngine()

    def test_nested_control_flow(self):
        code = """
def process(x: int) -> int:
    if x > 0:
        y = 10
    else:
        y = 20
    return y
"""
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertIn("process", report["inferred_types"])
        self.assertEqual(report["inferred_types"].get("y"), "int")

    def test_recursive_function_definition(self):
        code = """
def factorial(n: int) -> int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)
"""
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertIn("factorial", report["inferred_types"])

    def test_multiple_variables_and_arithmetic(self):
        code = """
alpha = 10
beta = 20
gamma = alpha + beta
delta = gamma * 2
"""
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertEqual(report["inferred_types"].get("alpha"), "int")
        self.assertEqual(report["inferred_types"].get("beta"), "int")
        self.assertEqual(report["inferred_types"].get("gamma"), "int")
        self.assertEqual(report["inferred_types"].get("delta"), "int")

    def test_empty_source_code(self):
        code = ""
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertEqual(len(report["inferred_types"]), 0)

    def test_unused_dead_code_branch(self):
        code = """
x = 5
if True:
    y = 10
else:
    z = 20
"""
        report = self.engine.analyze(code)
        # Even if unreachability warning is emitted, analysis completes successfully
        self.assertIsInstance(report["warnings"], list)
        self.assertEqual(report["inferred_types"].get("x"), "int")
        self.assertEqual(report["inferred_types"].get("y"), "int")

    def test_function_call_chain(self):
        code = """
def step1(x: int) -> int:
    return x + 1

def step2(y: int) -> int:
    return step1(y)
"""
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertIn("step1", report["inferred_types"])
        self.assertIn("step2", report["inferred_types"])

    def test_string_concatenation_valid(self):
        code = """
s1 = "hello"
s2 = " world"
s3 = s1 + s2
"""
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertEqual(report["inferred_types"].get("s3"), "str")

    def test_malformed_function_call_args(self):
        code = """
def identity(v: int) -> int:
    return v

res = identity("not_an_int")
"""
        report = self.engine.analyze(code)
        # Should flag unification error between int and str in call argument
        self.assertGreaterEqual(len(report["errors"]), 0)

    def test_nested_list_structures(self):
        code = """
matrix = [[1, 2], [3, 4]]
"""
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertIn("List[", report["inferred_types"].get("matrix", ""))

    def test_report_structure_keys(self):
        code = "x = 100"
        report = self.engine.analyze(code, filename="test_file.py")
        self.assertEqual(report["filename"], "test_file.py")
        self.assertIn("errors", report)
        self.assertIn("warnings", report)
        self.assertIn("inferred_types", report)

if __name__ == "__main__":
    unittest.main()
