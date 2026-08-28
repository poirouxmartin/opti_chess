#!/usr/bin/env python3
"""Download Lichess evaluation database, extract 50k positions for opti_chess eval testing.

Output: tests/lichess_evals.txt in FEN|eval_cp format (one position per line).

Usage:
    python scripts/download_lichess_evals.py [--count N] [--min-depth D]
"""

import argparse
import json
import os
import sys
import urllib.request
import zstandard as zstd

URL = "https://database.lichess.org/lichess_db_eval.jsonl.zst"
CACHE_DIR = os.path.join(os.path.dirname(__file__), "..", "tests", ".cache")
CACHE_FILE = os.path.join(CACHE_DIR, "lichess_db_eval.jsonl.zst")


def download_with_progress(url: str, dest: str, max_bytes: int = 0) -> None:
    """Download a file showing progress. If max_bytes > 0, stop after that many bytes."""
    print(f"Downloading {url} ...")
    print(f"This is a large file. {'Stopping after ' + str(max_bytes // (1024*1024)) + ' MB.' if max_bytes else 'It may take a while.'}")
    req = urllib.request.Request(url, headers={"User-Agent": "opti_chess/benchmark"})
    resp = urllib.request.urlopen(req, timeout=600)
    total = int(resp.headers.get("Content-Length", 0))
    downloaded = 0
    chunk_size = 1024 * 1024  # 1MB chunks
    with open(dest, "wb") as f:
        while True:
            chunk = resp.read(chunk_size)
            if not chunk:
                break
            f.write(chunk)
            downloaded += len(chunk)
            if max_bytes > 0 and downloaded >= max_bytes:
                print(f"\n  Stopped at {downloaded / (1024*1024):.1f} MB (limit reached)")
                break
            if total > 0:
                pct = downloaded * 100 / total
                mb = downloaded / (1024 * 1024)
                total_mb = total / (1024 * 1024)
                print(f"\r  {mb:.1f}/{total_mb:.1f} MB ({pct:.0f}%)", end="", flush=True)
            else:
                mb = downloaded / (1024 * 1024)
                print(f"\r  {mb:.1f} MB", end="", flush=True)
    print()


def classify_position(fen: str) -> str:
    """Classify a position by game phase."""
    board = fen.split()
    # Count pieces (simplified)
    piece_map = {}
    for char in fen.split()[0]:
        if char.isalpha():
            piece_map[char] = piece_map.get(char, 0) + 1

    total_pieces = sum(piece_map.values())
    queens = piece_map.get('Q', 0) + piece_map.get('q', 0)

    if total_pieces <= 10:
        return "endgame"
    elif total_pieces <= 16 or queens == 0:
        return "middlegame"
    else:
        return "opening"


def extract_evals(count: int, min_depth: int = 20, max_depth: int = 50) -> list:
    """Extract evaluations from the Lichess eval database."""
    evals = []
    total_read = 0
    total_skipped = 0

    print(f"  Extracting {count} positions (depth {min_depth}-{max_depth})...")

    dctx = zstd.ZstdDecompressor()
    with open(CACHE_FILE, "rb") as f:
        reader = dctx.stream_reader(f)
        text_wrapper = __import__('io').TextIOWrapper(reader, encoding="utf-8")

        for line in text_wrapper:
            total_read += 1
            if total_read % 100000 == 0:
                print(f"    scanned {total_read:,}, kept {len(evals)}", flush=True)

            if len(evals) >= count:
                break

            try:
                data = json.loads(line.strip())
            except json.JSONDecodeError:
                total_skipped += 1
                continue

            fen = data.get("fen", "")
            if not fen:
                total_skipped += 1
                continue

            evals_list = data.get("evals", [])
            if not evals_list:
                total_skipped += 1
                continue

            # Get evaluation with highest depth
            best_eval = None
            best_depth = 0
            for ev in evals_list:
                depth = ev.get("depth", 0)
                if depth >= min_depth and depth <= max_depth and depth > best_depth:
                    pvs = ev.get("pvs", [])
                    if pvs:
                        best_eval = pvs[0]  # First PV
                        best_depth = depth

            if best_eval is None:
                total_skipped += 1
                continue

            # Get eval in centipawns (or mate)
            cp = best_eval.get("cp")
            mate = best_eval.get("mate")

            if cp is not None:
                eval_cp = cp
                eval_type = "cp"
            elif mate is not None:
                # Convert mate to large cp value
                eval_cp = 30000 if mate > 0 else -30000
                eval_type = "mate"
            else:
                total_skipped += 1
                continue

            # Classify position
            phase = classify_position(fen)

            evals.append((fen, eval_cp, phase, best_depth))

    print(f"  done: scanned {total_read:,}, kept {len(evals)}, skipped {total_skipped:,}")
    return evals


def main():
    parser = argparse.ArgumentParser(description="Download Lichess evals for opti_chess")
    parser.add_argument("--count", type=int, default=50000,
                        help="Number of positions to extract (default: 50000)")
    parser.add_argument("--min-depth", type=int, default=20,
                        help="Minimum evaluation depth (default: 20)")
    parser.add_argument("--max-depth", type=int, default=50,
                        help="Maximum evaluation depth (default: 50)")
    parser.add_argument("--out", default=None,
                        help="Output file (default: tests/lichess_evals.txt)")
    args = parser.parse_args()

    os.makedirs(CACHE_DIR, exist_ok=True)

    # Download if not cached (only download 1GB - enough for 50k+ positions)
    if not os.path.exists(CACHE_FILE):
        # Only download first 1GB to get enough positions quickly
        download_with_progress(URL, CACHE_FILE, max_bytes=1024 * 1024 * 1024)
    else:
        size_mb = os.path.getsize(CACHE_FILE) / (1024 * 1024)
        print(f"Using cached {CACHE_FILE} ({size_mb:.1f} MB)")

    # Extract evals
    print(f"Extracting {args.count} evaluations (depth {args.min_depth}-{args.max_depth})...")
    evals = extract_evals(args.count, args.min_depth, args.max_depth)

    if not evals:
        print("ERROR: No evaluations found!", file=sys.stderr)
        sys.exit(1)

    # Write output
    if args.out:
        out_path = args.out
    else:
        out_path = os.path.join(os.path.dirname(__file__), "..", "tests", "lichess_evals.txt")

    with open(out_path, "w") as f:
        f.write("# FEN|eval_cp|phase|depth\n")
        for fen, eval_cp, phase, depth in evals:
            f.write(f"{fen}|{eval_cp}|{phase}|{depth}\n")

    print(f"Wrote {len(evals)} evaluations to {out_path}")

    # Stats by phase
    phase_counts = {}
    for _, _, phase, _ in evals:
        phase_counts[phase] = phase_counts.get(phase, 0) + 1
    print("  By phase:")
    for phase, cnt in sorted(phase_counts.items()):
        print(f"    {phase}: {cnt}")

    # Eval distribution
    eval_values = [e[1] for e in evals]
    if eval_values:
        print(f"  Eval distribution: min={min(eval_values)}, max={max(eval_values)}, "
              f"median={sorted(eval_values)[len(eval_values)//2]}")


if __name__ == "__main__":
    main()
