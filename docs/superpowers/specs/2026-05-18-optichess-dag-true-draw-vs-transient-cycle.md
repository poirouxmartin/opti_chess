# PROMPT — OptiChess #11 Plan B: distinguish a TRUE forced draw from a TRANSIENT DAG search-graph cycle

> Self-contained brief. Assume zero prior context. This is the **core unsolved DAG-soundness problem**. Read `opti_chess/CLAUDE.md` (perf is priority #1), `opti_chess/BUGFIXES.md` #11, `opti_chess/ALGORITHMS.md`, and the memory file `project-tt-main-search-state` before designing. Do **brainstorming/design first** (no guessed path-count patch — that already failed once, see "Reverted attempts").

## The bug (one sentence)

With the transposition **DAG** enabled (toggle `g_tt_node_dag`, GUI key **O**, `main_gui.h:313-316`), the engine cannot reliably tell a **genuinely forced draw by repetition** (fortress / perpetual) apart from a **transient cycle in the MCTS search graph of a winning position**, producing two opposite failures.

## Two faces of the same problem (repro positions)

1. **False win in a dead draw.** `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` (canonical transposition test) and a K+P(h-pawn)-vs-K theoretical draw. DAG OFF → instantly draw (correct). DAG ON → thinks it **wins** by circling indirectly; advancing moves crashes or corrupts (the crash/corruption is a *separate deferred* bug, see "Out of scope").
2. **Phantom draw in a win.** A "K vs K + blocked pawns" winning position: DAG ON shows the win first (correct), then the eval **drifts to draw**; navigating moves flips draw↔win. (Observed after the reverted Option D; the underlying conflation is the point.)

## Verified facts (do not re-derive wrong)

- `Board::get_zobrist_key()` (`board.cpp:6996`) XORs **side-to-move** (`_player_key`, `:7044-7045`), **castling** (`:7037`), **en-passant** (`:7041`). So the repetition key is position+trait+castling+ep. A **triangulation** (same board, opposite trait) does **NOT** collide. The earlier "twofold-too-aggressive-on-triangulation" theory was **false** — do not revisit it.
- Repetition predicate: `position_is_draw_by_repetition(const PositionHistory&, Board&, uint8_t limit=search_repetition_limit)` at `exploration.cpp:129`, body `position_history_count(path,board)+1 >= limit`. `search_repetition_limit=2` (`:7`, twofold, aggressive search prune — engine-wide, used in tree mode too). `display_repetition_limit=3` (`:16`, threefold/FIDE, used only for DAG **display** cuts in `get_exploration_variants`/`get_main_depth` — a separate, already-validated fix; keep).
- Path history is a single owned `PositionHistory` threaded by pointer with O(1) push/pop (`PathScope`) from work item #7 / Plan B-1. Under MCTS+DAG one root→leaf **traversal** is a walk of a **graph**: the same position+trait can recur in one traversal via **transposition** (a graph cycle), which is **not** a game draw.

## Current code state (post-revert, builds `EXITCODE=0`)

Tip of branch `feature/tt-main-search`. Functional state = commit `c749698`. Present mechanisms:
- **§3 structural recheck** `exploration.cpp:768` — `if (g_tt_node_dag && position_is_draw_by_repetition(branch_history, *child->_board))` in `explore_random_child`: on a path-local repetition it does **not descend** (cuts the cycle), counts the iteration, emits a canonical draw via `*path_local_eval = dag_draw_eval()` (`:795`; `dag_draw_eval()` at `:154` mirrors `Board::evaluate` draw branch `board.cpp:1551-1564`, position-independent, no shared mutation), and adds the move to `DagExcl` (opt-3). **This structural "don't descend" is correct.**
- **opt-3 anti-spin** `DagExcl` (`exploration.h:26`): stack-local per `grogros_zero` frame; a §3-cut edge is excluded from re-selection for the rest of that call (kills the spin). Sound, keep.
- **opt-1 partial backup** `exploration.cpp:809` — if the parent's best move ∈ `DagExcl`, back up `dag_draw_eval()`; persist to `this->_deep_evaluation` only if `_parent_count<=1` (`:822`). This only covers the *direct* cyclic edge at one frame and never bubbles a deep draw to the root through shared nodes (documented residual).
- Bug 2 model A (per-edge `_propagated_nodes`, decoupled) and the display threefold are intact and validated — out of scope here.

## Reverted attempts (learn from these — do NOT repeat)

- **Option D** (`f7a5227`, reverted `1e5461f`): made `pick_random_child`/`get_best_score_move`/`get_move_scores` path-aware — scored a child as a draw when `position_is_draw_by_repetition(path,child)` on the current traversal. **Failed because** it conflated "this MCTS/DAG traversal cycles here" (a graph-search artifact correctly handled by the §3 structural cut) with "this position is a forced draw value". In a winning position whose search graph naturally has cycles, winning children got draw-scored → phantom draw.
- **Threefold-for-value** (`71ed691`, reverted `1393c6e`): raised the DAG draw threshold 2→3. Mis-justified band-aid (built on the wrong triangulation model). A path-count threshold is **not** the right axis.

## The actual problem to solve

Define and apply a sound criterion for **"the side to move is in a genuinely forced repetition draw"** (no progress is possible — every line either repeats or transposes back, i.e. the cycle is *unavoidable*), as opposed to **"this particular MCTS traversal happened to revisit a position via transposition"** (benign; structurally cut, but the position may be winning). Then decide:
- **Where** to apply it: the §3 *structural* decision ("don't descend" — already correct) vs the *value/selection* (the part that broke). Likely the value must come from whether the cyclic edge is *forced*, not from a traversal recurrence.
- **How** to bubble a genuinely-forced-draw value to the root through **shared** nodes without mutating shared `_deep_evaluation` (the hard part — see invariant 772183a).

## Hard invariants / constraints (non-negotiable)

- **772183a invariant:** never write a path-local value (`_deep_evaluation`, `ChildLink`, `node_map`) onto a node/edge that may be shared (`_parent_count>1`). Path-local truth must travel by return value / stack, never persisted on shared structures.
- **OFF byte-identical:** when `g_tt_node_dag==false`, behaviour must be byte-identical to tree mode (tree mode plays these endgames correctly — confirmed by user). All new logic must be `g_tt_node_dag`-gated.
- **Performance is priority #1** (`opti_chess/CLAUDE.md`): `pick_random_child`/`get_node_score`/`grogros_zero` are the hottest paths. No allocation; O(1) per edge; profile any change. The B-1 O(1) threaded `PositionHistory` is available.
- Verification is a **manual user gate** (no automated search-behavior tests; the assistant cannot run the raylib GUI). Acceptance = user runs the two repro positions + DAG OFF identical. Build with MSBuild `opti_chess.sln` Debug x64, expect `EXITCODE=0`.

## Out of scope here (deferred, tracked in memory; do not fix in this effort)

`play_move_keep` (`gui.cpp:886`) × DAG re-root corruption cluster (buffer-full mismatch, "no moves in grogros_zero" spam, post-move mispricing, corrupted board on scrolling variations); Bug E (UI crash fixed by PC sleep/wake); standalone bug #1 (`get_main_depth` returns 0 before a heavy TT-hit node on a fresh search). Keep them in mind for interactions but solve them separately.

## Pointers

- Spec: `docs/superpowers/specs/2026-05-17-optichess-plan-b-dag-design.md` (§3 = repetition soundness; §6 = `_nodes`). 
- Plans: `docs/superpowers/plans/2026-05-18-optichess-plan-b2-dag.md`, `2026-05-18-optichess-bug1-opt1-path-local-draw-backup.md` (the negamax-return-value direction was flagged here as the "proper" path — re-evaluate it against this corrected problem statement).
- Memory: `project-tt-main-search-state` (full chronological state, decisions, what was reverted and why).
- Commit-message rule: English, conventional, ASCII, `(#11)`, **no AI attribution** (`opti_chess/CLAUDE.md`).

## Suggested first step

Brainstorm the *criterion* (forced/unavoidable repetition vs transient transposition) and *where it lives* before any code. Candidate angles to evaluate, not prescribe: (a) detect "no progress move exists" at the repeating node (all children repeat/transpose back) → forced draw; (b) negamax-style per-traversal return value carrying a *proven* forced-draw, distinct from a mere recurrence; (c) keep the §3 structural cut as-is and change only how/whether its value is trusted for backup/selection. Decide soundness under sharing + perf before implementing.
