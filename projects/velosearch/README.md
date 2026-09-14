# VeloSearch: Zero-Dependency Hybrid Vector-Lexical Search Engine

Welcome to **VeloSearch**, a lightweight, zero-dependency hybrid search engine written in pure Python 3.12. VeloSearch combines the precision of BM25 lexical search with the semantic capabilities of Locality Sensitive Hashing (LSH) dense vector search, integrated with a robust boolean query parser and merged using Reciprocal Rank Fusion (RRF).

---

## Architecture

VeloSearch is designed with a modular, local-first architecture that prioritizes low latency, minimal memory overhead, and ease of deployment. The system consists of five core components:

1. **Lexical Indexer**: Implements a custom English Porter Stemmer and tokenizer to normalize text, building an inverted index with positional postings lists. This allows for precise keyword matching and phrase queries.
2. **Dense Vector Indexer (LSH)**: Uses Locality Sensitive Hashing (LSH) with deterministic random projections to map high-dimensional vectors into discrete bucket hashes. This enables fast approximate nearest neighbor (ANN) search with exact cosine similarity fallback.
3. **Boolean Query Parser**: Implements a Shunting-Yard parser to convert infix boolean expressions (supporting nested `AND`, `OR`, and `NOT` operators) into postfix notation, evaluating them against the inverted index to filter candidate documents.
4. **Rank Fusion Engine (RRF)**: Merges the ranked results from the lexical and dense vector search engines using Reciprocal Rank Fusion (RRF), ensuring balanced and highly relevant hybrid search results.
5. **Storage & Serialization**: Provides a custom binary serialization format with CRC32 integrity verification, ensuring that the index state can be safely persisted to and loaded from disk.

```
+-------------------------------------------------------------------------+
|                               VeloSearch                                |
+-------------------------------------------------------------------------+
                                     |
        +----------------------------+----------------------------+
        |                                                         |
        v                                                         v
+-----------------------+                                 +-----------------------+
|     Lexical Index     |                                 |      Dense Index      |
|  - Porter Stemmer     |                                 |  - LSH Projections    |
|  - Positional Postings|                                 |  - Hamming Distance   |
|  - BM25 Scoring       |                                 |  - Cosine Similarity  |
+-----------------------+                                 +-----------------------+
        |                                                         |
        +----------------------------+----------------------------+
                                     | (RRF Rank Fusion)
                                     v
                        +-------------------------+
                        |     Hybrid Results      |
                        +-------------------------+
```

---

## Algorithms and Data Structures

### 1. Custom Porter Stemmer
To normalize words, VeloSearch implements Step 1 (1a, 1b, and 1c) of the Porter Stemming algorithm. This handles common English suffixes such as plural forms (`-sses` to `-ss`, `-ies` to `-i`, `-s` to empty), past tenses (`-eed` to `-ee`, `-ed` to empty), and continuous tenses (`-ing` to empty), along with double consonant reduction (e.g., `running` to `run`).

### 2. Shunting-Yard Boolean Parser
Boolean queries are parsed using the Shunting-Yard algorithm, which converts infix expressions (e.g., `(cat AND dog) OR NOT bird`) into Reverse Polish Notation (postfix). The postfix expression is then evaluated using a stack-based evaluator against the document sets retrieved from the inverted index.

### 3. BM25 Scoring
Lexical search relevance is calculated using the standard BM25 scoring formula:
$$\text{Score}(D, Q) = \sum_{q \in Q} \text{IDF}(q) \cdot \frac{f(q, D) \cdot (k_1 + 1)}{f(q, D) + k_1 \cdot \left(1 - b + b \cdot \frac{|D|}{\text{avgdl}}\right)}$$
Where IDF is calculated with a smoothing term to guarantee positive values:
$$\text{IDF}(q) = \ln\left(1.0 + \frac{N - n(q) + 0.5}{n(q) + 0.5}\right)$$

### 4. Locality Sensitive Hashing (LSH)
For dense vector search, VeloSearch projects high-dimensional vectors onto $K$ random hyperplanes generated deterministically using a seeded random number generator. Each projection yields a single bit (1 if the dot product is positive, 0 otherwise), forming a $K$-bit integer hash. Documents are bucketed by their hash values. Queries search neighboring buckets within a configurable Hamming distance threshold before falling back to exact cosine similarity calculation.

### 5. Reciprocal Rank Fusion (RRF)
To combine lexical and dense search ranks, VeloSearch uses Reciprocal Rank Fusion:
$$\text{RRF}(d) = \sum_{m \in M} \frac{1}{k + r_m(d)}$$
Where $r_m(d)$ is the 1-based rank of document $d$ in search system $m$, and $k$ is a constant (default: 60) that dampens the influence of high-ranking outliers.

---

## Invariants

- **Vector Dimension Consistency**: Every document added to the index must have a dense vector matching the configured dimension of the engine. Any mismatch raises a `ValueError` immediately.
- **CRC32 Integrity Verification**: The binary serialization format includes a 4-byte CRC32 checksum of the pickled payload. When loading an index, the checksum is recalculated and verified; any mismatch or corruption raises an exception.
- **Index Consistency under Deletion**: Deleting a document completely removes its terms from the inverted index, its vector from the LSH buckets, and its metadata from the document store, ensuring no stale references remain.

---

## Build and Run

VeloSearch requires Python 3.12 or higher and has **zero external dependencies**.

### Initializing an Index
```bash
python3 -m src.cli init --index my_index.idx --dimension 4 --num-hashes 4
```

### Adding a Document
```bash
python3 -m src.cli add --index my_index.idx --id 1 --text "machine learning and artificial intelligence" --vector "1.0,0.0,0.0,0.0" --metadata '{"category": "AI"}'
```

### Searching the Index
```bash
python3 -m src.cli search --index my_index.idx --query "learning" --vector "0.9,0.1,0.0,0.0" --boolean "NOT neural" --top-n 5
```

### Getting Index Info
```bash
python3 -m src.cli info --index my_index.idx
```

---

## Testing

To run the unit and integration tests, execute the following commands:

```bash
python3 -m unittest discover -s tests
```

Both test suites verify index consistency, boolean query parsing, BM25 scoring, LSH indexing, RRF rank merging, and binary serialization integrity.

---

## Security and Privacy

- **Local-First**: VeloSearch runs entirely on your local machine. No data is sent to external APIs or cloud services.
- **No Arbitrary Code Execution**: Serialization uses standard library `pickle` wrapped in a custom binary envelope with CRC32 verification to prevent loading corrupted or tampered index files.
- **Deterministic Behavior**: All random projections are seeded, ensuring identical index structures across runs.

---

## Performance Characteristics

- **Lexical Indexing**: $O(M \cdot L)$ where $M$ is the number of documents and $L$ is the average document length.
- **LSH Querying**: $O(B + C \cdot D)$ where $B$ is the number of buckets searched, $C$ is the number of candidate documents in those buckets, and $D$ is the vector dimension.
- **Memory Footprint**: Extremely lightweight as all data structures are stored in-memory during execution and serialized to a compact binary file on disk.

---

## Limitations

- **In-Memory Execution**: The entire index must fit into RAM during search operations.
- **Single-Node**: VeloSearch does not support distributed clustering or sharding out of the box.
- **Approximate Search**: LSH is an approximate nearest neighbor search algorithm; while highly efficient, it may occasionally miss the absolute nearest neighbor in exchange for speed.
