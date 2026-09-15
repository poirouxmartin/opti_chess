# TECH_DEBT.md — opti_chess

## Debt ledger

| ID | date | severity | file:line | description | fix plan |
|----|------|----------|-----------|-------------|----------|
| TD-001 | 2026-09-15 | medium | opti_chess/exploration_diag.cpp:867,913 (`node_map`) | `node_map` (`thread_local robin_map<uint64_t,Node*>`) has NO capacity cap, unlike the TT (eviction sweep since audit A2, `zobrist.cpp:146`). It only grows in DAG mode (`g_tt_node_dag`, default OFF) and is cleared on epoch change / DEL / `reset_buffers`. If DAG becomes default (bug #4 track) or long sessions run with `O` on, the heap grows unbounded. | Mirror the TT policy: cap at a pool-sized N entries with the same deterministic 1/8-stride eviction sweep; add a probe test asserting `size() <= cap` after a long DAG run. |
