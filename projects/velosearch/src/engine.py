# src/engine.py
"""
VeloSearch: A zero-dependency hybrid vector-lexical search engine.
Features:
- Custom Porter Stemmer and Tokenizer
- Positional Inverted Index
- BM25 Scoring Engine
- Locality Sensitive Hashing (LSH) Dense Vector Index
- Reciprocal Rank Fusion (RRF) Rank Merger
- Boolean Query Parser (AND, OR, NOT)
- On-disk binary serialization with CRC32 integrity verification
"""

import math
import pickle
import re
import struct
import zlib
import random

# --- Custom Porter Stemmer ---

def is_vowel(char: str) -> bool:
    return char in "aeiouy"

def has_vowel(word: str) -> bool:
    return any(is_vowel(c) for c in word)

def stem(word: str) -> str:
    """
    A robust implementation of Step 1 of the Porter Stemmer.
    Handles common suffixes like 'sses', 'ies', 'ss', 's', 'eed', 'ing', 'ed', and 'y'.
    """
    word = word.lower().strip()
    if len(word) <= 2:
        return word
    
    # Step 1a
    if word.endswith("sses"):
        word = word[:-2]
    elif word.endswith("ies"):
        word = word[:-2]  # flies -> fli
    elif word.endswith("ss"):
        pass
    elif word.endswith("s") and not word.endswith("us") and not word.endswith("is") and not word.endswith("as"):
        word = word[:-1]
        
    # Step 1b
    if word.endswith("eed"):
        stem_part = word[:-3]
        if has_vowel(stem_part):
            word = stem_part + "ee"
    elif word.endswith("ing"):
        stem_part = word[:-3]
        if has_vowel(stem_part):
            word = stem_part
            # cleanup double consonants
            if len(word) >= 2 and word[-1] == word[-2] and word[-1] not in "lsz":
                word = word[:-1]
            elif word.endswith("at") or word.endswith("bl") or word.endswith("iz"):
                word += "e"
    elif word.endswith("ed"):
        stem_part = word[:-2]
        if has_vowel(stem_part):
            word = stem_part
            if len(word) >= 2 and word[-1] == word[-2] and word[-1] not in "lsz":
                word = word[:-1]
            elif word.endswith("at") or word.endswith("bl") or word.endswith("iz"):
                word += "e"
                
    # Step 1c
    if word.endswith("y") and len(word) > 1 and is_vowel(word[-2]):
        pass
    elif word.endswith("y") and len(word) > 1:
        word = word[:-1] + "i"
        
    return word

# --- Tokenizer ---

def tokenize(text: str) -> list[str]:
    """
    Splits text by non-alphanumeric characters, lowercases, and stems each token.
    """
    if not text:
        return []
    tokens = re.findall(r'[a-zA-Z0-9]+', text.lower())
    return [stem(t) for t in tokens if t]

# --- Boolean Query Parser ---

def tokenize_query(query_str: str) -> list[str]:
    """
    Tokenizes a boolean query string, preserving operators and parentheses.
    """
    if not query_str:
        return []
    # Match parentheses, operators, or any non-whitespace sequence
    tokens = re.findall(r'\(|\)|AND|OR|NOT|[^\s()]+', query_str)
    return tokens

def infix_to_postfix(tokens: list[str]) -> list[str]:
    """
    Converts an infix boolean query expression to postfix (Reverse Polish Notation)
    using the Shunting-Yard algorithm.
    """
    precedence = {"NOT": 3, "AND": 2, "OR": 1}
    output = []
    stack = []
    
    for token in tokens:
        if token in precedence:
            while stack and stack[-1] in precedence and precedence[stack[-1]] >= precedence[token]:
                output.append(stack.pop())
            stack.append(token)
        elif token == "(":
            stack.append(token)
        elif token == ")":
            while stack and stack[-1] != "(":
                if not stack:
                    break # Mismatched parentheses
                output.append(stack.pop())
            if stack and stack[-1] == "(":
                stack.pop()
        else:
            # Normalize and stem search terms
            stemmed = stem(token.lower())
            output.append(f"TERM:{stemmed}")
            
    while stack:
        op = stack.pop()
        if op not in ("(", ")"):
            output.append(op)
    return output

def evaluate_postfix(postfix: list[str], all_doc_ids: set[int], inverted_index: dict[str, dict[int, list[int]]]) -> set[int]:
    """
    Evaluates a postfix boolean query expression against the inverted index.
    """
    stack = []
    for token in postfix:
        if token.startswith("TERM:"):
            term = token[5:]
            matching = set(inverted_index.get(term, {}).keys())
            stack.append(matching)
        elif token == "NOT":
            if not stack:
                stack.append(set())
                continue
            operand = stack.pop()
            stack.append(all_doc_ids - operand)
        elif token == "AND":
            if len(stack) < 2:
                stack.append(set())
                continue
            right = stack.pop()
            left = stack.pop()
            stack.append(left & right)
        elif token == "OR":
            if len(stack) < 2:
                stack.append(set())
                continue
            right = stack.pop()
            left = stack.pop()
            stack.append(left | right)
            
    if not stack:
        return set()
    return stack[-1]

# --- BM25 Scoring Engine ---

def score_bm25(
    query_terms: list[str],
    doc_ids: set[int],
    inverted_index: dict[str, dict[int, list[int]]],
    doc_lengths: dict[int, int],
    avg_doc_length: float,
    doc_count: int,
    k1: float = 1.5,
    b: float = 0.75
) -> dict[int, float]:
    """
    Calculates BM25 scores for a set of documents given a list of query terms.
    """
    scores = {}
    for term in query_terms:
        postings = inverted_index.get(term, {})
        nq = len(postings)
        if nq == 0:
            continue
            
        # Standard BM25 IDF formula with 1.0 added to ensure positive values
        idf = math.log(1.0 + (doc_count - nq + 0.5) / (nq + 0.5))
        
        for doc_id in doc_ids:
            if doc_id in postings:
                tf = len(postings[doc_id])
                doc_len = doc_lengths.get(doc_id, 0)
                denom = tf + k1 * (1.0 - b + b * (doc_len / (avg_doc_length or 1.0)))
                score = idf * (tf * (k1 + 1.0)) / denom
                scores[doc_id] = scores.get(doc_id, 0.0) + score
    return scores

# --- Locality Sensitive Hashing (LSH) Dense Vector Index ---

class LSHIndex:
    """
    A Locality Sensitive Hashing (LSH) index for high-dimensional dense vectors.
    Uses random projections to map vectors to bucket hashes, enabling fast approximate
    nearest neighbor search with exact cosine similarity fallback.
    """
    def __init__(self, dimension: int, num_hashes: int = 16, seed: int = 42):
        self.dimension = dimension
        self.num_hashes = num_hashes
        self.seed = seed
        self.buckets = {} # hash (int) -> list of (doc_id, vector)
        self.doc_vectors = {} # doc_id -> vector
        
        # Generate deterministic random projection vectors
        rng = random.Random(seed)
        self.projections = []
        for _ in range(num_hashes):
            v = [rng.gauss(0, 1) for _ in range(dimension)]
            norm = math.sqrt(sum(x*x for x in v))
            if norm > 0:
                v = [x / norm for x in v]
            self.projections.append(v)
            
    def compute_hash(self, vector: list[float]) -> int:
        val = 0
        for i, proj in enumerate(self.projections):
            dot = sum(x * y for x, y in zip(vector, proj))
            if dot >= 0:
                val |= (1 << i)
        return val
        
    def add(self, doc_id: int, vector: list[float]):
        self.delete(doc_id)
        
        # Normalize vector to unit length
        norm = math.sqrt(sum(x*x for x in vector))
        if norm > 0:
            vector = [x / norm for x in vector]
        else:
            vector = [0.0] * self.dimension
            
        h = self.compute_hash(vector)
        if h not in self.buckets:
            self.buckets[h] = []
        self.buckets[h].append((doc_id, vector))
        self.doc_vectors[doc_id] = vector
        
    def delete(self, doc_id: int):
        if doc_id in self.doc_vectors:
            old_vec = self.doc_vectors.pop(doc_id)
            h = self.compute_hash(old_vec)
            if h in self.buckets:
                self.buckets[h] = [item for item in self.buckets[h] if item[0] != doc_id]
                if not self.buckets[h]:
                    del self.buckets[h]
                    
    def query(self, query_vector: list[float], top_n: int = 10, max_hamming: int = 2) -> list[tuple[int, float]]:
        norm = math.sqrt(sum(x*x for x in query_vector))
        if norm > 0:
            query_vector = [x / norm for x in query_vector]
        else:
            query_vector = [0.0] * self.dimension
            
        q_hash = self.compute_hash(query_vector)
        candidates = []
        
        # Search buckets within Hamming distance threshold
        for h, items in self.buckets.items():
            xor_val = q_hash ^ h
            hamming_dist = bin(xor_val).count('1')
            if hamming_dist <= max_hamming:
                candidates.extend(items)
                
        # Fallback to all vectors if no candidates found in nearby buckets
        if not candidates:
            for h, items in self.buckets.items():
                candidates.extend(items)
                
        results = []
        seen = set()
        for doc_id, vec in candidates:
            if doc_id in seen:
                continue
            seen.add(doc_id)
            # Cosine similarity of normalized vectors is their dot product
            sim = sum(x * y for x, y in zip(query_vector, vec))
            results.append((doc_id, sim))
            
        results.sort(key=lambda x: x[1], reverse=True)
        return results[:top_n]

# --- Reciprocal Rank Fusion (RRF) ---

def reciprocal_rank_fusion(ranks: list[list[int]], k: int = 60) -> list[tuple[int, float]]:
    """
    Combines multiple ranked lists of document IDs using Reciprocal Rank Fusion (RRF).
    """
    rrf_scores = {}
    for rank_list in ranks:
        for rank_idx, doc_id in enumerate(rank_list):
            rank = rank_idx + 1
            rrf_scores[doc_id] = rrf_scores.get(doc_id, 0.0) + 1.0 / (k + rank)
    return sorted(rrf_scores.items(), key=lambda x: x[1], reverse=True)

# --- VeloSearchEngine ---

class VeloSearchEngine:
    """
    The main VeloSearch engine coordinating lexical indexing, dense vector indexing,
    boolean query parsing, and hybrid search merging.
    """
    def __init__(
        self, 
        dimension: int = 128,
        num_hashes: int = 16,
        seed: int = 42,
        k1: float = 1.5,
        b: float = 0.75,
        rrf_k: int = 60
    ):
        self.dimension = dimension
        self.num_hashes = num_hashes
        self.seed = seed
        self.k1 = k1
        self.b = b
        self.rrf_k = rrf_k
        
        self.documents = {} # doc_id -> {"text": str, "vector": list[float], "metadata": dict}
        self.inverted_index = {} # term -> {doc_id: [positions]}
        self.doc_lengths = {} # doc_id -> int
        self.lsh_index = LSHIndex(dimension, num_hashes, seed)
        
    def add_document(self, doc_id: int, text: str, vector: list[float], metadata: dict = None):
        """
        Adds or updates a document in both the lexical and dense vector indexes.
        """
        if len(vector) != self.dimension:
            raise ValueError(f"Vector dimension mismatch. Expected {self.dimension}, got {len(vector)}")
            
        if metadata is None:
            metadata = {}
            
        # Handle in-place updates by deleting first
        if doc_id in self.documents:
            self.delete_document(doc_id)
            
        # Lexical indexing
        tokens = tokenize(text)
        self.doc_lengths[doc_id] = len(tokens)
        for pos, token in enumerate(tokens):
            if token not in self.inverted_index:
                self.inverted_index[token] = {}
            if doc_id not in self.inverted_index[token]:
                self.inverted_index[token][doc_id] = []
            self.inverted_index[token][doc_id].append(pos)
            
        # Dense indexing
        self.lsh_index.add(doc_id, vector)
        
        # Store document metadata
        self.documents[doc_id] = {
            "text": text,
            "vector": vector,
            "metadata": metadata
        }
        
    def delete_document(self, doc_id: int):
        """
        Removes a document from all indexes.
        """
        if doc_id not in self.documents:
            return
            
        # Remove from inverted index
        text = self.documents[doc_id]["text"]
        tokens = tokenize(text)
        for token in set(tokens):
            if token in self.inverted_index:
                if doc_id in self.inverted_index[token]:
                    del self.inverted_index[token][doc_id]
                if not self.inverted_index[token]:
                    del self.inverted_index[token]
                    
        # Remove from doc lengths
        if doc_id in self.doc_lengths:
            del self.doc_lengths[doc_id]
            
        # Remove from LSH index
        self.lsh_index.delete(doc_id)
        
        # Remove from documents
        del self.documents[doc_id]
        
    def get_avg_doc_length(self) -> float:
        if not self.doc_lengths:
            return 0.0
        return sum(self.doc_lengths.values()) / len(self.doc_lengths)
        
    def search(
        self, 
        query_text: str = None, 
        query_vector: list[float] = None, 
        boolean_query: str = None, 
        top_n: int = 10
    ) -> list[dict]:
        """
        Performs hybrid search combining BM25 lexical search and LSH dense vector search,
        optionally filtered by a boolean query expression.
        """
        # 1. Boolean Filtering
        filtered_doc_ids = set(self.documents.keys())
        if boolean_query:
            tokens = tokenize_query(boolean_query)
            postfix = infix_to_postfix(tokens)
            filtered_doc_ids = evaluate_postfix(postfix, filtered_doc_ids, self.inverted_index)
            
        if not filtered_doc_ids:
            return []
            
        # 2. Lexical Search (BM25)
        bm25_ranked = []
        if query_text:
            query_tokens = tokenize(query_text)
            bm25_scores = score_bm25(
                query_tokens,
                filtered_doc_ids,
                self.inverted_index,
                self.doc_lengths,
                self.get_avg_doc_length(),
                len(self.documents),
                self.k1,
                self.b
            )
            bm25_ranked = [doc_id for doc_id, _ in sorted(bm25_scores.items(), key=lambda x: x[1], reverse=True)]
            
        # 3. Dense Search (LSH)
        lsh_ranked = []
        if query_vector is not None:
            if len(query_vector) != self.dimension:
                raise ValueError(f"Query vector dimension mismatch. Expected {self.dimension}, got {len(query_vector)}")
            lsh_results = self.lsh_index.query(query_vector, top_n=len(self.documents))
            lsh_ranked = [doc_id for doc_id, _ in lsh_results if doc_id in filtered_doc_ids]
            
        # 4. Rank Fusion
        if query_text and query_vector is not None:
            # Hybrid search using RRF
            rrf_results = reciprocal_rank_fusion([bm25_ranked, lsh_ranked], self.rrf_k)
            final_doc_ids = [doc_id for doc_id, _ in rrf_results][:top_n]
        elif query_text:
            # Lexical only
            final_doc_ids = bm25_ranked[:top_n]
        elif query_vector is not None:
            # Dense only
            final_doc_ids = lsh_ranked[:top_n]
        else:
            # Boolean filter only or empty query
            final_doc_ids = sorted(list(filtered_doc_ids))[:top_n]
            
        # Construct results
        results = []
        for doc_id in final_doc_ids:
            doc = self.documents[doc_id]
            results.append({
                "id": doc_id,
                "text": doc["text"],
                "metadata": doc["metadata"]
            })
        return results
        
    def save(self, filepath: str):
        """
        Serializes the engine state to a binary file with CRC32 integrity verification.
        """
        state = {
            "dimension": self.dimension,
            "num_hashes": self.num_hashes,
            "seed": self.seed,
            "k1": self.k1,
            "b": self.b,
            "rrf_k": self.rrf_k,
            "documents": self.documents,
            "inverted_index": self.inverted_index,
            "doc_lengths": self.doc_lengths,
            "lsh_index_buckets": self.lsh_index.buckets,
            "lsh_index_doc_vectors": self.lsh_index.doc_vectors,
            "lsh_index_projections": self.lsh_index.projections
        }
        payload = pickle.dumps(state)
        crc = zlib.crc32(payload) & 0xffffffff
        length = len(payload)
        
        with open(filepath, "wb") as f:
            f.write(b"VELO")
            f.write(struct.pack(">II", crc, length))
            f.write(payload)
            
    @classmethod
    def load(cls, filepath: str) -> "VeloSearchEngine":
        """
        Loads the engine state from a binary file, verifying CRC32 integrity.
        """
        with open(filepath, "rb") as f:
            magic = f.read(4)
            if magic != b"VELO":
                raise ValueError("Invalid file format: missing VELO magic bytes")
            header = f.read(8)
            if len(header) < 8:
                raise ValueError("Invalid file format: truncated header")
            crc, length = struct.unpack(">II", header)
            payload = f.read(length)
            if len(payload) < length:
                raise ValueError("Invalid file format: truncated payload")
                
            actual_crc = zlib.crc32(payload) & 0xffffffff
            if actual_crc != crc:
                raise ValueError("Integrity check failed: CRC32 mismatch")
                
            state = pickle.loads(payload)
            engine = cls(
                dimension=state["dimension"],
                num_hashes=state["num_hashes"],
                seed=state["seed"],
                k1=state["k1"],
                b=state["b"],
                rrf_k=state["rrf_k"]
            )
            engine.documents = state["documents"]
            engine.inverted_index = state["inverted_index"]
            engine.doc_lengths = state["doc_lengths"]
            engine.lsh_index.buckets = state["lsh_index_buckets"]
            engine.lsh_index.doc_vectors = state["lsh_index_doc_vectors"]
            engine.lsh_index.projections = state["lsh_index_projections"]
            return engine
