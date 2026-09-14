# src/cli.py
import argparse
import json
import sys
import os
from src.engine import VeloSearchEngine

def parse_vector(vector_str: str) -> list[float]:
    try:
        return [float(x.strip()) for x in vector_str.split(",")]
    except ValueError:
        raise argparse.ArgumentTypeError("Vector must be a comma-separated list of floats.")

def main():
    parser = argparse.ArgumentParser(
        description="VeloSearch: A zero-dependency hybrid vector-lexical search engine CLI."
    )
    parser.add_argument(
        "--index",
        default="velosearch.idx",
        help="Path to the VeloSearch index file (default: velosearch.idx)"
    )
    
    subparsers = parser.add_subparsers(dest="command", help="Subcommands")
    
    # Init command
    init_parser = subparsers.add_parser("init", help="Initialize a new index file")
    init_parser.add_argument("--dimension", type=int, default=128, help="Vector dimension")
    init_parser.add_argument("--num-hashes", type=int, default=16, help="Number of LSH hashes")
    init_parser.add_argument("--seed", type=int, default=42, help="Random seed for projections")
    
    # Add command
    add_parser = subparsers.add_parser("add", help="Add or update a document in the index")
    add_parser.add_argument("--id", type=int, required=True, help="Unique document ID")
    add_parser.add_argument("--text", type=str, required=True, help="Document text content")
    add_parser.add_argument("--vector", type=parse_vector, required=True, help="Comma-separated vector floats")
    add_parser.add_argument("--metadata", type=str, default="{}", help="JSON string of metadata")
    
    # Delete command
    delete_parser = subparsers.add_parser("delete", help="Delete a document from the index")
    delete_parser.add_argument("--id", type=int, required=True, help="Document ID to delete")
    
    # Search command
    search_parser = subparsers.add_parser("search", help="Search the index")
    search_parser.add_argument("--query", type=str, help="Lexical query text")
    search_parser.add_argument("--vector", type=parse_vector, help="Dense query vector (comma-separated floats)")
    search_parser.add_argument("--boolean", type=str, help="Boolean query expression (e.g. 'cat AND dog')")
    search_parser.add_argument("--top-n", type=int, default=10, help="Number of results to return")
    
    # Info command
    subparsers.add_parser("info", help="Print index statistics")
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        sys.exit(1)
        
    if args.command == "init":
        engine = VeloSearchEngine(
            dimension=args.dimension,
            num_hashes=args.num_hashes,
            seed=args.seed
        )
        engine.save(args.index)
        print(f"Successfully initialized empty index at '{args.index}' with dimension {args.dimension}.")
        return
        
    # For other commands, we need to load the index
    if not os.path.exists(args.index):
        print(f"Error: Index file '{args.index}' does not exist. Run 'init' first.", file=sys.stderr)
        sys.exit(1)
        
    try:
        engine = VeloSearchEngine.load(args.index)
    except Exception as e:
        print(f"Error loading index: {e}", file=sys.stderr)
        sys.exit(1)
        
    if args.command == "add":
        try:
            metadata = json.loads(args.metadata)
        except json.JSONDecodeError:
            print("Error: Invalid JSON string for metadata.", file=sys.stderr)
            sys.exit(1)
            
        try:
            engine.add_document(
                doc_id=args.id,
                text=args.text,
                vector=args.vector,
                metadata=metadata
            )
            engine.save(args.index)
            print(f"Successfully added/updated document ID {args.id}.")
        except Exception as e:
            print(f"Error adding document: {e}", file=sys.stderr)
            sys.exit(1)
            
    elif args.command == "delete":
        if args.id not in engine.documents:
            print(f"Warning: Document ID {args.id} not found in index.", file=sys.stderr)
            sys.exit(0)
        engine.delete_document(args.id)
        engine.save(args.index)
        print(f"Successfully deleted document ID {args.id}.")
        
    elif args.command == "search":
        try:
            results = engine.search(
                query_text=args.query,
                query_vector=args.vector,
                boolean_query=args.boolean,
                top_n=args.top_n
            )
            print(json.dumps(results, indent=2))
        except Exception as e:
            print(f"Error during search: {e}", file=sys.stderr)
            sys.exit(1)
            
    elif args.command == "info":
        print(f"Index Path: {args.index}")
        print(f"Dimension: {engine.dimension}")
        print(f"Num Hashes: {engine.num_hashes}")
        print(f"Seed: {engine.seed}")
        print(f"Total Documents: {len(engine.documents)}")
        print(f"Average Document Length: {engine.get_avg_doc_length():.2f} tokens")
        print(f"Vocabulary Size: {len(engine.inverted_index)} terms")

if __name__ == "__main__":
    main()
