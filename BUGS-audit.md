# GUI bug audit (2026-10-15, branch dev/poiroux/gui-audit)

Scope: GUI mode (opti_chess.exe). Bugs were found by running the GUI under scripts/gui_drive.ps1 and by reviewing gui.cpp, main_gui.h, game_tree.cpp and board_display.cpp.
Severity: Critical (corrupts the game or crashes), High, Medium, Low. Status: Fixed (commit) / Open.
File:line references are against the pre-fix main (6be90ab).

## How to reproduce: the GUI driver

- `powershell -ExecutionPolicy Bypass -File scripts\gui_drive.ps1 -Script scripts\gui_scenarios\<file>.txt [-Exe path] [-OutDir dir] [-KeepOpen]`
- Launches the GUI (kills running opti_chess.exe), sets the client area (default 1200x800), sends real keyboard/mouse input, and checks `opti_chess_debug.log` (next to the exe).
- Exit code 1 if any check fails. Artifacts (screenshots, debug_slice.log) go to `build\gui_audit\`.
- The GUI logs `[geom]` (board geometry/orientation) and `[pos] <tag> fen=` on every position change. The driver maps squares with the former and asserts on the latter.
- Promotion picker: tiles run from the promotion square toward the centre, in the order Q R B N.

Step grammar:

| Step | Meaning |
|---|---|
| `wait <ms>` | pause |
| `key <combo>` | press key (G, CTRL+G, SHIFT+P, LEFT, DEL, KP1, F11...) |
| `hold <combo> <ms>` | hold key |
| `click <sq>` / `rclick <sq>` | left / right click on square |
| `drag <from> <to>` / `rdrag` | left / right drag |
| `clickxy <x> <y>` | click at pixel |
| `resize <w> <h>` | resize client area |
| `minimize` / `restore` | window state |
| `shot <name>` | screenshot |
| `clip <text>` | clipboard text |
| `expect_fen <fen>` | assert position |
| `expect_log <regex>` / `expect_nolog <regex>` | assert log |
| `alive` | assert process running |

Scenarios:

- `moves_history.txt`: click/drag moves, castling, LEFT/RIGHT history, side variation, CTRL+N.
- `fen_ep_promo.txt`: FEN paste, en passant (including after DEL), promotions for both colours, flipped board.
- `engine_window.txt`: CTRL+G analysis, arrow clicks, J, bot play, game over, resize/minimize, misc keys.

Gotcha: synthetic keys need the real scan code (GLFW maps by scan code). Without it, arrows arrive as keypad keys.

## Results

| Scenario | Before (f394aad) | After (tip) |
|---|---|---|
| moves_history | 13 pass / 2 fail | 15 / 0 |
| fen_ep_promo | 10 / 1 | 11 / 0 |
| engine_window | 10 / 0 | 10 / 0 |

Note: engine_window passes on both. The arrow-click iterator bug (G-06) and the reset slot reuse (G-02) are latent (UB / timing). They are confirmed by code and by the log line "new board reuses the old root board slot", which appeared on every CTRL+N before the fix.

## Bugs

| ID | Severity | Where | Symptom / repro | Status |
|---|---|---|---|---|
| G-01 | Critical | game_tree.cpp:107-110 (select_previous_node) | LEFT aliases the live board to the game-tree node storage; the next move is played on it and rewrites history. Repro: moves_history [25]/[27]: after castling, LEFT LEFT then ...d6 gives a castled position with d6; LEFT then shows it again. | Fixed (fix(gui): keep the live board owned...) |
| G-02 | Critical | gui.cpp:2479-2502 (reset_game) | Frees the old root board, takes a new slot (the same one: logged on every CTRL+N), then resets/recycles the old root. The new board is wiped and its slot freed while live (double ownership). | Fixed |
| G-03 | High | gui.cpp:1036 (play_move_keep) | `_current_node->_board = *_board` (implicit operator=) copies the pool slot index into the game tree; later recycling could free a live slot. | Fixed |
| G-04 | High | main_gui.h:473 (DEL), gui.cpp:1521 (run_puzzle_headless), gui.cpp:1152 | Node::reset wipes the root board's Zobrist key, history and en passant column. After DEL, en passant is illegal (fen_ep_promo [13]); repetition detection is lost; puzzle search starts from a zero key. | Fixed |
| G-05 | High | board_fen.cpp:229-235 (Board::from_fen) | Engine code writes GUI state on every parse: flips orientation, sets the PGN initial FEN, and replaces the game tree. This affects the V/L probe boards and puzzle worker threads (data race), and leaks the old tree root. A black-to-move FEN paste flipped the board unexpectedly. | Fixed (load_FEN does the GUI side) |
| G-06 | High | gui.cpp:1980-1989 (draw, arrow click) | play_move_keep clears _grogros_arrows while the range-for iterates it (UB, possible crash or second move). | Fixed (fix(gui): harden input...) |
| G-07 | High | board_display.cpp:305, :323 | stoi on clock text box: an empty box (Backspace, then Enter or click away) throws, and the GUI crashes. | Fixed |
| G-08 | Medium | gui.cpp:695-739 (resize_GUI) | Every resize leaks 26 textures and 26 images. Minimizing builds 0-size textures ([geom] w=0 h=0). No minimum window size. | Fixed |
| G-09 | Medium | main_gui.h:800-810 | Once a game is drawn, the block rewrites root _iterations/_is_terminal every frame, concurrently with a running worker. The worker keeps spinning on the finished position. | Fixed (guarded, worker stopped at game over) |
| G-10 | Medium | main_gui.h (no flag fall) | Clocks go negative and the game continues. | Fixed (clocks stop at 0) |
| G-11 | Medium | main_gui.h:524-574 | Keypad clock presets fire while typing keypad digits into a clock box. | Fixed |
| G-12 | Low | game_tree.cpp:156 | Partial Board copy keeps _got_moves but not _moves: SAN label may be computed from a stale list. | Fixed |
| G-13 | Low | main_gui.h:596/604 | Shift-J played two moves on its first frame. | Fixed |
| G-14 | Low | main_gui.h:256/426 | CTRL-B (boost) also printed bitboards and re-inited buffers. | Fixed |
| G-15 | Low | main_gui.h:446 | Shift-Enter used IsKeyPressed(SHIFT): 10-iteration mode unreachable. | Fixed |
| G-16 | Low | main_gui.h:319 vs :327 | S saved data/test.txt, L loads data/text.txt. | Fixed |
| G-17 | Low | main_gui.h:132-141 | F11 toggled maximize and exclusive fullscreen (changes monitor resolution). | Fixed (borderless toggle) |
| G-18 | Low | main_gui.h comments | Key comments named wrong keys (A/Q, Space, W). | Fixed |
| G-19 | Medium | main_gui.h:282 and :410 | CTRL-Q bound twice: toggles full binding AND starts live analysis on chess.com. | Open (needs a decision on which keeps CTRL-Q) |
| G-20 | Medium | main_gui.h:647-700 | UP/DOWN "GrogrosFish" toggles rename the player, but nothing plays (grogrosfish calls commented out at :843-847). | Open (remove the toggle or wire it to a bot) |
| G-21 | Low | gui.cpp:2579 | GrogrosZero bot only moves while the clock runs (Space). Intended per comments, but silent for users. | Fixed (status text "BOT WAITS: start the clock (SPACE)") |
| G-22 | Low | main_gui.h:106-107 | Startup `new Board()` / `new Node()` leaked once (replaced by reset_game). | Won't fix (one-time, a few hundred bytes; deleting them safely needs Node ownership changes) |
| G-23 | Low | main_gui.h (all handlers) | Only LEFT CTRL is checked; C/X/V/E/R/F also fire with CTRL. | Fixed for RIGHT CTRL (ctrl_down()). C/X/V with CTRL kept on purpose (CTRL-C/X/V copy/paste work); E/R/F/S have no CTRL binding. |
| G-24 | Low | gui.cpp:492-497 (get_pos_from_GUI) | The board's top edge pixel maps to row 8 (out of range, ignored). | Fixed (half-open tiles) |
| G-25 | Info | TODO_list.txt:185,264 | Low FPS when idle (12 FPS) not reproduced or measured here. | Open |
| G-26 | High | gui.cpp:2565-2586 (compare_arrows) | Comparator passed to std::ranges::sort returned true for unrelated or equal arrows (not a strict weak ordering: undefined behaviour, can read out of range). | Fixed (iterations, then length, then squares) |
| G-27 | High | scripts/gui_drive.ps1 (test harness) | On an AZERTY layout the driver sent `key A` as the physical key raylib reads as KEY_Q, so the scenario opened chess.com in the default browser on every run (and W/Z, M were also mis-sent). | Fixed: fixed US scan codes for letters/digits; the driver sets OPTI_NO_BROWSER=1 and Q then logs `[url] ...` instead of calling OpenURL |

## Verification

- Harness (`scripts/harness.ps1 -Full`): perft 7/7, gtest, UCI, bench signature 8a8961478e14dd47 unchanged. The engine is not modified apart from removing GUI side effects from Board::from_fen.
- Single-thread perf A/B (scripts/perf_compare.ps1, base = main 6be90ab): 3 interleaved rounds, medians: perft 79.37 (base) vs 79.24 Mnps (tip), noise; bench 64591 vs 67324 nodes/s (+4.2%, plausibly from_fen no longer allocating a game-tree node per parse; not claimed as an optimisation). All rounds 7/7, signature 8a8961478e14dd47.

## Remaining

- G-19, G-20 (need a decision), G-25 (not measured).
- The G-21/G-23/G-24/G-26/G-27 changes passed the harness but the scenarios were not re-run: the workstation was locked (LogonUI), which swallows synthetic input. The driver now exits with code 3 and "desktop is locked" in that case. Re-run the three scenarios once unlocked; engine_window.txt now checks that Q logs `[url]` instead of opening a browser.
- Shared-tree race (docs/BENCHMARKS.md) unchanged.
- The arrow-click (G-06) and slot-reuse (G-02) fixes are verified by code and by the absence of the log line, not by a deterministic crash repro.
