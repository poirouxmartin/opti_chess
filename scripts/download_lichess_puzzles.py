#!/usr/bin/env python3
"""Download Lichess puzzle database, extract 2000 puzzles for opti_chess benchmark.

Output: tests/lichess_2000.txt in FEN|SAN|name format (one puzzle per line).

Usage:
    python scripts/download_lichess_puzzles.py [--count N] [--min-rating R] [--max-rating R]
"""

import argparse
import csv
import io
import os
import sys
import urllib.request
import zstandard as zstd
import chess
import chess.pgn

URL = "https://database.lichess.org/lichess_db_puzzle.csv.zst"
CACHE_DIR = os.path.join(os.path.dirname(__file__), "..", "tests", ".cache")
CACHE_FILE = os.path.join(CACHE_DIR, "lichess_db_puzzle.csv")


def download_with_progress(url: str, dest: str) -> None:
    """Download a file showing progress."""
    print(f"Downloading {url} ...")
    req = urllib.request.Request(url, headers={"User-Agent": "opti_chess/benchmark"})
    resp = urllib.request.urlopen(req, timeout=300)
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
            if total > 0:
                pct = downloaded * 100 / total
                mb = downloaded / (1024 * 1024)
                total_mb = total / (1024 * 1024)
                print(f"\r  {mb:.1f}/{total_mb:.1f} MB ({pct:.0f}%)", end="", flush=True)
            else:
                mb = downloaded / (1024 * 1024)
                print(f"\r  {mb:.1f} MB", end="", flush=True)
    print()


def uci_to_san(fen: str, uci_move: str) -> str:
    """Convert a UCI move string to SAN notation."""
    board = chess.Board(fen)
    move = chess.Move.from_uci(uci_move)
    if move not in board.legal_moves:
        return ""
    return board.san(move)


def stream_decompress_rows(csv_path: str):
    """Stream-decompress a .zstd CSV file row by row."""
    dctx = zstd.ZstdDecompressor()
    with open(csv_path, "rb") as f:
        reader = dctx.stream_reader(f)
        text_wrapper = io.TextIOWrapper(reader, encoding="utf-8")
        csv_reader = csv.reader(text_wrapper)
        header = next(csv_reader, None)
        if header:
            print(f"  CSV header: {header[:5]}...")
        for row in csv_reader:
            yield row


def extract_puzzles(count: int, min_rating: int, max_rating: int,
                    themes: list = None, per_theme: int = 0) -> list:
    """Extract puzzles from the Lichess CSV cache.
    
    If themes is provided, only keep puzzles matching one of the listed themes.
    If per_theme > 0, limit to that many per theme (and ignore count).
    """
    puzzles = []
    by_theme_count = {}
    total_read = 0
    total_skipped = 0
    target = per_theme * len(themes) if (themes and per_theme > 0) else count

    for row in stream_decompress_rows(CACHE_FILE):
        total_read += 1
        if len(row) < 5:
            total_skipped += 1
            continue

        puzzle_id, fen, moves_str, rating_str, *_ = row

        # Theme is field index 7 (Themes column)
        theme = row[7].strip() if len(row) > 7 else ""
        # Take first theme as primary
        primary_theme = theme.split()[0] if theme else "unknown"

        try:
            rating = int(rating_str)
        except ValueError:
            total_skipped += 1
            continue

        if rating < min_rating or rating > max_rating:
            total_skipped += 1
            continue

        # Filter by theme if specified
        if themes:
            if primary_theme not in themes:
                total_skipped += 1
                continue
            # Per-theme limit
            if per_theme > 0 and by_theme_count.get(primary_theme, 0) >= per_theme:
                total_skipped += 1
                continue

        moves = moves_str.strip().split()
        if len(moves) < 2:
            total_skipped += 1
            continue

        # Apply the opponent's first move to get the puzzle position
        board = chess.Board(fen)
        try:
            first_move = chess.Move.from_uci(moves[0])
            if first_move not in board.legal_moves:
                total_skipped += 1
                continue
            board.push(first_move)
        except (ValueError, chess.InvalidMoveError):
            total_skipped += 1
            continue

        puzzle_fen = board.fen()

        # Convert the player's response (move 2) to SAN
        try:
            second_move = chess.Move.from_uci(moves[1])
            if second_move not in board.legal_moves:
                total_skipped += 1
                continue
            san = board.san(second_move)
        except (ValueError, chess.InvalidMoveError):
            total_skipped += 1
            continue

        name = f"lichess_{puzzle_id}_r{rating}"
        puzzles.append((puzzle_fen, san, name, primary_theme))
        by_theme_count[primary_theme] = by_theme_count.get(primary_theme, 0) + 1

        if per_theme > 0:
            # Stop when all themes have enough
            if all(by_theme_count.get(t, 0) >= per_theme for t in themes):
                break
        elif len(puzzles) >= count:
            break

        if total_read % 100000 == 0:
            print(f"  scanned {total_read:,}, kept {len(puzzles)}, skipped {total_skipped:,}", flush=True)

    print(f"  done: scanned {total_read:,}, kept {len(puzzles)}, skipped {total_skipped:,}")
    return puzzles


def main():
    parser = argparse.ArgumentParser(description="Download Lichess puzzles for benchmark")
    parser.add_argument("--count", type=int, default=2000, help="Number of puzzles to extract (default: 2000)")
    parser.add_argument("--min-rating", type=int, default=1000, help="Min puzzle rating (default: 1000)")
    parser.add_argument("--max-rating", type=int, default=2500, help="Max puzzle rating (default: 2500)")
    parser.add_argument("--themes", nargs="*", default=None,
                        help="Filter by Lichess themes (e.g. fork pin mate advantage)")
    parser.add_argument("--per-theme", type=int, default=250,
                        help="Puzzles per theme when --themes is set (default: 250)")
    parser.add_argument("--out", default=None,
                        help="Output file (default: tests/lichess_2000.txt or tests/lichess_themed.txt)")
    args = parser.parse_args()

    os.makedirs(CACHE_DIR, exist_ok=True)

    # Download if not cached
    if not os.path.exists(CACHE_FILE):
        download_with_progress(URL, CACHE_FILE)
    else:
        size_mb = os.path.getsize(CACHE_FILE) / (1024 * 1024)
        print(f"Using cached {CACHE_FILE} ({size_mb:.1f} MB)")

    # Extract puzzles
    if args.themes:
        print(f"Extracting {args.per_theme} puzzles per theme: {args.themes}...")
        puzzles = extract_puzzles(0, args.min_rating, args.max_rating,
                                  themes=args.themes, per_theme=args.per_theme)
    else:
        print(f"Extracting {args.count} puzzles (rating {args.min_rating}-{args.max_rating})...")
        puzzles = extract_puzzles(args.count, args.min_rating, args.max_rating)

    if not puzzles:
        print("ERROR: No puzzles found!", file=sys.stderr)
        sys.exit(1)

    # Write output
    if args.out:
        out_path = args.out
    elif args.themes:
        out_path = os.path.join(os.path.dirname(__file__), "..", "tests", "lichess_themed.txt")
    else:
        out_path = os.path.join(os.path.dirname(__file__), "..", "tests", "lichess_2000.txt")
    with open(out_path, "w") as f:
        f.write("# FEN|SAN|name|theme\n")
        for fen, san, name, theme in puzzles:
            f.write(f"{fen}|{san}|{name}|{theme}\n")

    print(f"Wrote {len(puzzles)} puzzles to {out_path}")

    # Per-theme stats
    theme_counts = {}
    for _, _, _, theme in puzzles:
        theme_counts[theme] = theme_counts.get(theme, 0) + 1
    if theme_counts:
        print("  Themes:")
        for th, cnt in sorted(theme_counts.items(), key=lambda x: -x[1]):
            print(f"    {th}: {cnt}")

    # Stats
    ratings = []
    for row in stream_decompress_rows(CACHE_FILE):
        if len(row) >= 4:
            try:
                ratings.append(int(row[3]))
            except ValueError:
                pass
        if len(ratings) >= 50000:
            break
    if ratings:
        print(f"  Rating distribution of first 50k: min={min(ratings)}, max={max(ratings)}, "
              f"median={sorted(ratings)[len(ratings)//2]}")


if __name__ == "__main__":
    main()
