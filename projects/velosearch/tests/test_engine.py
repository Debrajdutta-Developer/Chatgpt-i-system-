# tests/test_engine.py
import unittest
import os
import tempfile
from src.engine import (
    stem, tokenize, tokenize_query, infix_to_postfix, evaluate_postfix,
    score_bm25, LSHIndex, reciprocal_rank_fusion, VeloSearchEngine
)

class TestVeloSearchEngine(unittest.TestCase):
    def test_stemmer(self):
        # 1. Test basic plural removal
        self.assertEqual(stem("cats"), "cat")
        # 2. Test 'sses' suffix
        self.assertEqual(stem("classes"), "class")
        # 3. Test 'ies' suffix
        self.assertEqual(stem("flies"), "fli")
        # 4. Test 'ing' suffix with double consonant reduction
        self.assertEqual(stem("running"), "run")
        # 5. Test 'eed' suffix
        self.assertEqual(stem("agreed"), "agree")
        # 6. Test 'ed' suffix
        self.assertEqual(stem("walked"), "walk")
        # 7. Test 'y' suffix replacement
        self.assertEqual(stem("happy"), "happi")
        # 8. Test short words are unchanged
        self.assertEqual(stem("it"), "it")

    def test_tokenizer(self):
        # 9. Test standard tokenization and normalization
        tokens = tokenize("The quick brown fox jumps over the lazy dog!")
        self.assertIn("quick", tokens)
        self.assertIn("brown", tokens)
        # 10. Test empty string tokenization
        self.assertEqual(tokenize(""), [])

    def test_boolean_parser_tokens(self):
        # 11. Test query tokenization
        tokens = tokenize_query("(cat AND dog) OR NOT bird")
        self.assertEqual(tokens, ["(", "cat", "AND", "dog", ")", "OR", "NOT", "bird"])

    def test_boolean_parser_postfix(self):
        # 12. Test infix to postfix conversion
        tokens = ["(", "cat", "AND", "dog", ")", "OR", "NOT", "bird"]
        postfix = infix_to_postfix(tokens)
        self.assertEqual(postfix, ["TERM:cat", "TERM:dog", "AND", "TERM:bird", "NOT", "OR"])

    def test_boolean_filtering(self):
        # 13. Test postfix evaluation
        inverted_index = {
            "cat": {1: [0], 2: [0]},
            "dog": {2: [1], 3: [0]},
            "bird": {3: [1]}
        }
        all_docs = {1, 2, 3}
        # Query: cat AND dog -> should be {2}
        postfix = ["TERM:cat", "TERM:dog", "AND"]
        res = evaluate_postfix(postfix, all_docs, inverted_index)
        self.assertEqual(res, {2})
        
        # Query: NOT bird -> should be {1, 2}
        postfix_not = ["TERM:bird", "NOT"]
        res_not = evaluate_postfix(postfix_not, all_docs, inverted_index)
        self.assertEqual(res_not, {1, 2})

    def test_bm25_indexing(self):
        # 14. Test BM25 scoring
        inverted_index = {
            "quick": {1: [0], 2: [0]},
            "brown": {1: [1]},
            "fox": {1: [2]}
        }
        doc_lengths = {1: 3, 2: 1}
        scores = score_bm25(
            query_terms=["quick"],
            doc_ids={1, 2},
            inverted_index=inverted_index,
            doc_lengths=doc_lengths,
            avg_doc_length=2.0,
            doc_count=2
        )
        self.assertTrue(len(scores) > 0)
        # Doc 2 is shorter, so its BM25 score for "quick" should be higher than Doc 1
        self.assertGreater(scores[2], scores[1])

    def test_lsh_indexing(self):
        # 15. Test LSH index creation and query
        lsh = LSHIndex(dimension=4, num_hashes=4, seed=42)
        lsh.add(1, [1.0, 0.0, 0.0, 0.0])
        lsh.add(2, [0.0, 1.0, 0.0, 0.0])
        
        # Query close to doc 1
        res = lsh.query([0.9, 0.1, 0.0, 0.0], top_n=2)
        self.assertEqual(res[0][0], 1)
        # 16. Test cosine similarity value is reasonable
        self.assertGreater(res[0][1], 0.8)

    def test_rrf_fusion(self):
        # 17. Test Reciprocal Rank Fusion
        ranks = [
            [1, 2, 3],
            [2, 1, 3]
        ]
        fused = reciprocal_rank_fusion(ranks, k=60)
        # Doc 1 and 2 should have the same score since their ranks are symmetric
        self.assertAlmostEqual(fused[0][1], fused[1][1])
        # 18. Doc 3 should be ranked last
        self.assertEqual(fused[2][0], 3)

    def test_deletions_and_updates(self):
        # 19. Test dynamic updates and deletions
        engine = VeloSearchEngine(dimension=4, num_hashes=4, seed=42)
        engine.add_document(1, "the quick brown fox", [1.0, 0.0, 0.0, 0.0])
        engine.add_document(1, "the lazy dog", [0.0, 1.0, 0.0, 0.0]) # Update
        
        # The old terms should be gone
        self.assertNotIn("fox", engine.inverted_index)
        # 20. The new terms should be present
        self.assertIn("dog", engine.inverted_index)

    def test_malformed_inputs(self):
        # 21. Test dimension mismatch
        engine = VeloSearchEngine(dimension=4, num_hashes=4, seed=42)
        with self.assertRaises(ValueError):
            engine.add_document(1, "test", [1.0, 0.0])
