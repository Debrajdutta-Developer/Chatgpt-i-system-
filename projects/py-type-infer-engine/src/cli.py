import argparse
import sys
import json
from typing import Optional
from src.engine import AnalysisEngine

def main() -> None:
    parser = argparse.ArgumentParser(
        description="PyTypeInfer Engine: Zero-dependency static type inference and abstract interpretation."
    )
    parser.add_argument("filepath", nargs="?", help="Path to the Python source file to analyze.")
    parser.add_argument("--code", help="Inline Python source code string to analyze.")
    parser.add_argument("--format", choices=["json", "text"], default="json", help="Output format for analysis results.")
    parser.add_argument("--output", help="Optional file path to write the diagnostic report.")
    args = parser.parse_args()

    source_code: Optional[str] = None
    filename: str = "<string>"

    if args.filepath:
        filename = args.filepath
        try:
            with open(filename, "r", encoding="utf-8") as f:
                source_code = f.read()
        except Exception as e:
            print(f"Error reading file {filename}: {e}", file=sys.stderr)
            sys.exit(1)
    elif args.code:
        source_code = args.code
    else:
        source_code = sys.stdin.read()

    engine = AnalysisEngine()
    report = engine.analyze(source_code, filename=filename)

    if args.format == "json":
        output_data = json.dumps(report, indent=2)
    else:
        lines = [
            f"File: {report['filename']}",
            f"Errors: {len(report['errors'])}",
            f"Warnings: {len(report['warnings'])}",
            f"Inferred Types: {len(report['inferred_types'])}"
        ]
        for err in report["errors"]:
            lines.append(f"  [ERROR] Line {err['line']}:{err['column']} - {err['message']}")
        for warn in report["warnings"]:
            lines.append(f"  [WARNING] Line {warn['line']}:{warn['column']} - {warn['message']}")
        output_data = "\n".join(lines)

    if args.output:
        try:
            with open(args.output, "w", encoding="utf-8") as f:
                f.write(output_data)
            print(f"Report successfully written to {args.output}")
        except Exception as e:
            print(f"Error writing output file {args.output}: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        print(output_data)

if __name__ == "__main__":
    main()
