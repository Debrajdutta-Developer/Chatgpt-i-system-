import unittest
from src.engine import AnalysisEngine, PrimType, ListType, DictType, FuncType, Interval

class TestEngineCore(unittest.TestCase):
    def setUp(self):
        self.engine = AnalysisEngine()

    def test_primitive_inference(self):
        code = "x = 42\ny = 'hello'"
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertEqual(report["inferred_types"].get("x"), "int")
        self.assertEqual(report["inferred_types"].get("y"), "str")

    def test_binary_op_type_conflict(self):
        code = "x = 10 + 'world'"
        report = self.engine.analyze(code)
        self.assertGreater(len(report["errors"]), 0)
        self.assertIn("Binary operation type mismatch", report["errors"][0]["message"])

    def test_reassignment_conflict(self):
        code = "a = 100\na = 'string_val'"
        report = self.engine.analyze(code)
        self.assertGreater(len(report["errors"]), 0)
        self.assertIn("Reassignment type conflict", report["errors"][0]["message"])

    def test_function_signature_inference(self):
        code = "def compute(a: int, b: int) -> int:\n    return a + b"
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertIn("compute", report["inferred_types"])
        self.assertIn("Callable", report["inferred_types"]["compute"])

    def test_list_type_inference(self):
        code = "items = [1, 2, 3]"
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertEqual(report["inferred_types"].get("items"), "List[int]")

    def test_heterogeneous_list_error(self):
        code = "items = [1, 'two', 3]"
        report = self.engine.analyze(code)
        self.assertGreater(len(report["errors"]), 0)
        self.assertIn("Heterogeneous list element type", report["errors"][0]["message"])

    def test_dict_type_inference(self):
        code = "mapping = {'a': 1, 'b': 2}"
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 0)
        self.assertIn("Dict[", report["inferred_types"].get("mapping", ""))

    def test_syntax_error_handling(self):
        code = "def broken_syntax("
        report = self.engine.analyze(code)
        self.assertEqual(len(report["errors"]), 1)
        self.assertIn("SyntaxError", report["errors"][0]["message"])

    def test_interval_domain(self):
        iv1 = Interval(0, 10)
        iv2 = Interval(5, 20)
        iv3 = iv1.join(iv2)
        self.assertEqual(iv3.min_val, 0)
        self.assertEqual(iv3.max_val, 20)

    def test_unification_primitives(self):
        t1 = PrimType("int")
        t2 = PrimType("int")
        # Should not raise
        self.engine.unify(t1, t2)
        t3 = PrimType("str")
        with self.assertRaises(TypeError):
            self.engine.unify(t1, t3)

if __name__ == "__main__":
    unittest.main()
