# tests/test_integration.py
import unittest
import os
import tempfile
import json
from src.engine import VeloSearchEngine

class TestVeloSearchIntegration(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.index_path = os.path.join(self.temp_dir.name, "test_index.idx")
        
    def tearDown(self):
        self.temp_dir.cleanup()
        
    def test_end_to_end_flow(self):
        # 1. Initialize engine
        engine = VeloSearchEngine(dimension=4, num_hashes=4, seed=100)
        
        # 2. Add documents
        engine.add_document(1, "machine learning and artificial intelligence", [1.0, 0.0, 0.0, 0.0], {"category": "AI"})
        engine.add_document(2, "deep learning and neural networks", [0.8, 0.2, 0.0, 0.0], {"category": "AI"})
        engine.add_document(3, "classical music and opera performances", [0.0, 0.0, 1.0, 0.0], {"category": "Music"})
        engine.add_document(4, "jazz music and saxophone solos", [0.0, 0.0, 0.8, 0.2], {"category": "Music"})
        
        # 3. Save to disk
        engine.save(self.index_path)
        self.assertTrue(os.path.exists(self.index_path)) # Assertion 1
        
        # 4. Load from disk
        loaded_engine = VeloSearchEngine.load(self.index_path)
        self.assertEqual(len(loaded_engine.documents), 4) # Assertion 2
        self.assertEqual(loaded_engine.dimension, 4) # Assertion 3
        
        # 5. Test Lexical Search
        lexical_results = loaded_engine.search(query_text="music")
        self.assertGreater(len(lexical_results), 0) # Assertion 4
        self.assertEqual(lexical_results[0]["id"], 3) # Assertion 5 (music is first in doc 3)
        
        # 6. Test Dense Search
        dense_results = loaded_engine.search(query_vector=[0.0, 0.0, 0.9, 0.1])
        self.assertGreater(len(dense_results), 0) # Assertion 6
        self.assertEqual(dense_results[0]["id"], 3) # Assertion 7 (closest to [0, 0, 1, 0])
        
        # 7. Test Hybrid Search (RRF)
        hybrid_results = loaded_engine.search(query_text="learning", query_vector=[0.9, 0.1, 0.0, 0.0])
        self.assertGreater(len(hybrid_results), 0) # Assertion 8
        self.assertIn(hybrid_results[0]["id"], [1, 2]) # Assertion 9
        
        # 8. Test Boolean Filtered Search
        # Query "learning" normally matches 1 and 2. But "NOT neural" excludes 2. So only 1 should match.
        filtered_results = loaded_engine.search(
            query_text="learning",
            boolean_query="NOT neural"
        )
        self.assertEqual(len(filtered_results), 1) # Assertion 10
        self.assertEqual(filtered_results[0]["id"], 1) # Assertion 11
        
        # 9. Test Integrity Verification
        # Corrupt the file
        with open(self.index_path, "r+b") as f:
            f.seek(12) # Seek past header
            f.write(b"\x00\x00\x00\x00") # Corrupt payload
            
        with self.assertRaises(Exception): # Assertion 12
            VeloSearchEngine.load(self.index_path)
