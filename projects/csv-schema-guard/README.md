# CSV Schema Guard

## Problem
Incoming CSV files frequently suffer from malformed rows, mismatched column counts, encoding glitches, or inconsistent date formats that break database import scripts, causing hours of debugging cryptic errors.

## Features
- **Drag & Drop CSV Inspection**: Client-side parsing with instant row and column metrics.
- **Interactive Visual Grid**: Highlights anomaly cells (missing columns, type mismatches, non-ASCII characters, whitespace issues).
- **Type Inference & Validation**: Automatically infers column types (integer, float, date, text) and flags violating cells.
- **One-Click Repairs**: Fix quotes, trim whitespace, normalize delimiters, replace non-ASCII characters, and fill missing cells.
- **Sanitized Export**: Download clean, valid CSV files instantly.

## Usage
1. Open `index.html` in any modern web browser.
2. Drag and drop a CSV file or use the file picker.
3. Configure expected column types and rules in the sidebar.
4. Inspect highlighted anomalies in the interactive grid.
5. Apply repairs and export the clean CSV.

## Privacy & Local-First
All data processing happens 100% locally in your browser using JavaScript and HTML5 APIs (FileReader, Web Workers / synchronous stream parser). No files, data, or metrics ever leave your machine.

## Limitations
- Very large files (>100MB) may consume significant browser memory depending on available hardware.
