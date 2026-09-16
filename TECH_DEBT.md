# TECH_DEBT.md — opti_chess

## Debt ledger

| ID | date | severity | file:line | description | fix plan |
|----|------|----------|-----------|-------------|----------|
| TD-001 | 2026-09-15 | medium | opti_chess/exploration_diag.cpp:867,913 (`node_map`) | `node_map` (`thread_local robin_map<uint64_t,Node*>`) has NO capacity cap, unlike the TT (eviction sweep since audit A2, `zobrist.cpp:146`). It only grows in DAG mode (`g_tt_node_dag`, default OFF) and is cleared on epoch change / DEL / `reset_buffers`. If DAG becomes default (bug #4 track) or long sessions run with `O` on, the heap grows unbounded. | Mirror the TT policy: cap at a pool-sized N entries with the same deterministic 1/8-stride eviction sweep; add a probe test asserting `size() <= cap` after a long DAG run. |
| TD-004 | 2026-09-16 | medium | opti_chess/board.h (`max_moves=100`, `EvalControls`) | DIET DONE 2026-09-16: `sizeof(Board)` 2056 → 648 B (-68%: -1024 caches SquareMap → caller-owned `EvalControls` threaded from `evaluate()`, -372 `_moves[224]`→`[100]`, `_got_moves` int→int_fast8_t). Proven: core 100/100, GATE-50 NODES-500/SEED-42 McNemar 42/42 zero flips (χ²=0), 50/50 identical chosen+scores, EvalNPS 94-175k/s ref. ACCEPTED RISK (user vote): cap 100 < record 218 — `add_move()` silently drops beyond 100 (storm perft still green: no node exceeds 100 there). Revisit if a real >100-move truncation is ever observed (then sidecar, not Board growth). |
