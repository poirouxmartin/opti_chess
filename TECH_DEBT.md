# TECH_DEBT.md — opti_chess

## Debt ledger

| ID | date | severity | file:line | description | fix plan |
|----|------|----------|-----------|-------------|----------|
| TD-001 | 2026-09-15 | medium | opti_chess/exploration_diag.cpp (`node_map`) | DIET DONE 2026-09-16: `enforce_node_map_cap()` mirrors the TT amortized policy (`zobrist.cpp:146`) — cap = node-pool length, 1/8-stride eviction at all 3 DAG insert sites (explore_new_move miss + 2 shared publishes). Erasing entries never touches Nodes (later probes miss and rebuild, tree-equivalent). Proven: `Puzzle.NodeMapStaysCapped` (flood cap+5000 nullptr-miss entries + 2k DAG search → size <= cap, 950 ms) + core 101/101. |
| TD-004 | 2026-09-16 | medium | opti_chess/board.h (`max_moves=100`, `EvalControls`) | DIET DONE 2026-09-16: `sizeof(Board)` 2056 → 648 B (-68%: -1024 caches SquareMap → caller-owned `EvalControls` threaded from `evaluate()`, -372 `_moves[224]`→`[100]`, `_got_moves` int→int_fast8_t). Proven: core 100/100, GATE-50 NODES-500/SEED-42 McNemar 42/42 zero flips (χ²=0), 50/50 identical chosen+scores, EvalNPS 94-175k/s ref. ACCEPTED RISK (user vote): cap 100 < record 218 — `add_move()` silently drops beyond 100 (storm perft still green: no node exceeds 100 there). Revisit if a real >100-move truncation is ever observed (then sidecar, not Board growth). |
