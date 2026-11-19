# TODO inventory - opti_chess

Scope A: case-sensitive TODO|FIXME|HACK|XXX in opti_chess/*.cpp, opti_chess/*.h, opti_chess/tests/*.cpp (build/ and _deps excluded). Hits: 229.

## A) Source comments

### opti_chess/board.cpp (19)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/board.cpp:724 | FIXME | // FIXME *** possible d'optimiser en coupant certaines directions? |
| opti_chess/board.cpp:1042 | TODO | // TODO: merge with the equivalents in the other functions? |
| opti_chess/board.cpp:1062 | TODO | // TODO: is there a cheaper order to scan the lines in, given the opponent is more likely to attack through the centre? |
| opti_chess/board.cpp:1205 | TODO | // TODO *** optimiser |
| opti_chess/board.cpp:1208 | TODO | // TODO: check whether this actually makes it faster |
| opti_chess/board.cpp:1221 | TODO | // TODO *** rendre plus efficace |
| opti_chess/board.cpp:1408 | TODO | // TODO |
| opti_chess/board.cpp:1410 | TODO | // TODO: decide whether to mirror everything make_move does (eval reset and so on) |
| opti_chess/board.cpp:1411 | TODO | // TODO: would a faster make_move_minimal be worth it? |
| opti_chess/board.cpp:1419 | TODO | // TODO: handle the special cases (castling, en passant, promotion?) |
| opti_chess/board.cpp:1562 | FIXME | // FIXME: would allocating a fresh board be faster, and safer memory-wise? |
| opti_chess/board.cpp:1585 | FIXME | // FIXME *** manque l'en-passant |
| opti_chess/board.cpp:1640 | FIXME | // FIXME: should this only count in pawn endgames? |
| opti_chess/board.cpp:2034 | FIXME | // FIXME: is it computed several times? |
| opti_chess/board.cpp:2317 | TODO | // TODO |
| opti_chess/board.cpp:2378 | FIXME | // FIXME: perhaps not. It makes some sense in the opening, but not in the endgame where the king is mobile |
| opti_chess/board.cpp:2548 | FIXME | // FIXME: how do we detect two pieces blocking each other, such as two adjacent rooks or a rook beside a knight? They currently do not count |
| opti_chess/board.cpp:2549 | TODO | // TODO: an alternative approach: |
| opti_chess/board.cpp:2565 | TODO | //// TODO *** to think about: why would we start with the pawns? |

### opti_chess/board.h (16)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/board.h:19 | TODO | // TODO: use them |
| opti_chess/board.h:223 | TODO | // TODO *** to be used |
| opti_chess/board.h:239 | TODO | // TODO: use this!! |
| opti_chess/board.h:251 | TODO | int row : 4; // TODO: would 3 be enough? |
| opti_chess/board.h:260 | TODO | // TODO *** use static constants so that this is precomputed |
| opti_chess/board.h:514 | TODO | // TODO *** add total value (-> move score?) |
| opti_chess/board.h:594 | TODO | //Array _array; // TODO use this |
| opti_chess/board.h:596 | TODO | // Bitboard!! (TODO) |
| opti_chess/board.h:603 | TODO | // TODO *** add masks for the controls |
| opti_chess/board.h:604 | TODO | // TODO *** magic bitboards to be used |
| opti_chess/board.h:606 | TODO | // TODO *** optional: cached king (will it replace the _white_king_pos?) |
| opti_chess/board.h:654 | TODO | // TODO *** put this in the nodes rather than in the boards? |
| opti_chess/board.h:675 | FIXME | // FIXME *** dummy variables for memory alignment, otherwise it creates very slow new vectors on every board creation |
| opti_chess/board.h:1115 | TODO | // TODO |
| opti_chess/board.h:1125 | TODO | // TODO *** write a more generic piece_safety? |
| opti_chess/board.h:1127 | TODO | // TODO *** faster move generation |

### opti_chess/eval_core.cpp (6)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/eval_core.cpp:404 | TODO | // TODO: damp short-term mobility in closed positions (multiply by |
| opti_chess/eval_core.cpp:426 | TODO | // TODO (review): central enemy outposts (e.g. Nd5 vs unmoved knights) |
| opti_chess/eval_core.cpp:491 | TODO | main_GUI._eval_components += "forks: TODO\n"; |
| opti_chess/eval_core.cpp:499 | TODO | // Pins (TODO: move them here) |
| opti_chess/eval_core.cpp:722 | TODO | // TODO: rewrite this |
| opti_chess/eval_core.cpp:766 | TODO | // *** TODO *** |

### opti_chess/eval_king.cpp (45)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/eval_king.cpp:32 | TODO | // TODO: factor mobility into king safety |
| opti_chess/eval_king.cpp:255 | TODO | constexpr float piece_overload_multiplicator = 1.0f; // TODO: put this to use |
| opti_chess/eval_king.cpp:342 | TODO | // TODO: extract a function for this |
| opti_chess/eval_king.cpp:478 | FIXME | // FIXME: is this actually useful? it may break more than it fixes |
| opti_chess/eval_king.cpp:494 | TODO | // TODO ********* |
| opti_chess/eval_king.cpp:500 | TODO | // TODO |
| opti_chess/eval_king.cpp:515 | TODO | // TODO ********* |
| opti_chess/eval_king.cpp:662 | TODO | main_GUI._eval_components += " + Rank weakness (TODO): " + to_string(b_rank_weakness); |
| opti_chess/eval_king.cpp:663 | TODO | main_GUI._eval_components += " + Mating nets (TODO): " + to_string(0); |
| opti_chess/eval_king.cpp:744 | TODO | main_GUI._eval_components += " + Rank weakness (TODO): " + to_string(w_rank_weakness); |
| opti_chess/eval_king.cpp:745 | TODO | main_GUI._eval_components += " + Mating nets (TODO): " + to_string(0); |
| opti_chess/eval_king.cpp:824 | FIXME | // FIXME: base this on the pawns only? |
| opti_chess/eval_king.cpp:916 | FIXME | if (b.in_check()) // FIXME? |
| opti_chess/eval_king.cpp:920 | FIXME | // FIXME *** why is another board used here? |
| opti_chess/eval_king.cpp:942 | FIXME | // FIXME: pas ouf |
| opti_chess/eval_king.cpp:963 | TODO | // TODO: replace with "does the move attack the king"? |
| opti_chess/eval_king.cpp:994 | FIXME | // FIXME: bof |
| opti_chess/eval_king.cpp:1090 | TODO | // TODO: use the controlled squares to find the fastest route; hard to do, but it could help a lot |
| opti_chess/eval_king.cpp:1092 | TODO | // TODO: take into account whether the pawn is passed |
| opti_chess/eval_king.cpp:1104 | TODO | // TODO |
| opti_chess/eval_king.cpp:1108 | TODO | // TODO |
| opti_chess/eval_king.cpp:1175 | FIXME | float w_proximity = w_distance == 0.0f ? 0.0f : w_pawn_value / w_distance; // FIXME: subtract instead of divide? |
| opti_chess/eval_king.cpp:1224 | FIXME | float w_proximity = w_distance == 0.0f ? 0.0f : w_pawn_value / w_distance; // FIXME: subtract instead of divide? |
| opti_chess/eval_king.cpp:1298 | FIXME | // FIXME: should only the piece count matter? |
| opti_chess/eval_king.cpp:1302 | TODO | // TODO: prendre en compte distance 2?? |
| opti_chess/eval_king.cpp:1613 | TODO | // Pawn: TODO, revisit |
| opti_chess/eval_king.cpp:1782 | TODO | else if (semi_defenses > 0) { // TODO: improve by counting the semi-defences? |
| opti_chess/eval_king.cpp:1797 | TODO | // TODO *** |
| opti_chess/eval_king.cpp:1812 | TODO | // TODO: should the search GrogrosZero has already done be taken into account? |
| opti_chess/eval_king.cpp:1816 | TODO | // TODO: should the number of available captures be taken into account? |
| opti_chess/eval_king.cpp:1818 | FIXME | // FIXME: improvable. When behind both materially and non-materially, it avoids regaining activity and pulling the non-material term back to |
| opti_chess/eval_king.cpp:1827 | TODO | // TODO: a proper formula for the uncertainty is still needed |
| opti_chess/eval_king.cpp:1859 | TODO | // TODO: opposed should reinforce itself, and non-opposed likewise |
| opti_chess/eval_king.cpp:2028 | FIXME | // FIXME: this may need to be applied before the uncertainty |
| opti_chess/eval_king.cpp:2042 | TODO | // TODO: pick the move from this score rather than from the evaluation? |
| opti_chess/eval_king.cpp:2120 | TODO | // TODO: tedious, but probably very strong |
| opti_chess/eval_king.cpp:2242 | TODO | // TODO |
| opti_chess/eval_king.cpp:2249 | FIXME | // FIXME: this does not cover everything; the king may fail to reach the pawn for other reasons, controlled squares among them |
| opti_chess/eval_king.cpp:2255 | TODO | // TODO: account for the +1 at the boundaries when the king is to move |
| opti_chess/eval_king.cpp:2298 | TODO | // TODO: dedicated functions for the distances to castling |
| opti_chess/eval_king.cpp:2326 | TODO | // TODO: could be improved when pieces other than the original bishop block the castle |
| opti_chess/eval_king.cpp:2338 | FIXME | // FIXME: fairly arbitrary. 0.9 when castling is available, decaying linearly to 0 otherwise, assuming the distance never exceeds 8 |
| opti_chess/eval_king.cpp:2546 | TODO | // TODO: consider only the safe moves |
| opti_chess/eval_king.cpp:2587 | TODO | // TODO: use a richer function accounting for the queen's placement and the enemy pieces around it |
| opti_chess/eval_king.cpp:2605 | TODO | // TODO: proximity to the king should carry a penalty |

### opti_chess/eval_misc.cpp (25)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/eval_misc.cpp:29 | TODO | // TODO: revisit, to model real threats more faithfully |
| opti_chess/eval_misc.cpp:30 | FIXME | // FIXME: penalty for unprotected pieces? |
| opti_chess/eval_misc.cpp:54 | TODO | // TODO: stop summing the defence of every piece? look only at undefended ones, otherwise the engine turns timid |
| opti_chess/eval_misc.cpp:55 | FIXME | // FIXME: x-ray attacks and defences are not accounted for |
| opti_chess/eval_misc.cpp:71 | TODO | // TODO: use constants for the redundant computations |
| opti_chess/eval_misc.cpp:74 | TODO | // TODO: replace the ifs with && |
| opti_chess/eval_misc.cpp:633 | TODO | // TODO: scale this by the pieces able to occupy it |
| opti_chess/eval_misc.cpp:1128 | TODO | // TODO: merge with other functions, so it stops handing out a flat bonus even when castling is terrible |
| opti_chess/eval_misc.cpp:1130 | TODO | // Look for pieces blocking the castle (and whether they can move? TODO), or enemy pieces controlling the squares |
| opti_chess/eval_misc.cpp:1143 | TODO | // TODO: improve this, there are special cases |
| opti_chess/eval_misc.cpp:1219 | TODO | // TODO *** ajouter win conditions |
| opti_chess/eval_misc.cpp:1333 | FIXME | // 8/PK6/3k4/8/8/8/8/8 w - - 0 79 : FIXME *** a8 ne devrait pas baisser le winnable! |
| opti_chess/eval_misc.cpp:1449 | TODO | // TODO: implement drawing chances for unbalanced material ratios in the endgame |
| opti_chess/eval_misc.cpp:1450 | TODO | // TODO: bishop against knight |
| opti_chess/eval_misc.cpp:1452 | TODO | // TODO: special pawn endgame case: king and pawn against lone king is a theoretical draw when the king stands in front of the enemy pawn |
| opti_chess/eval_misc.cpp:1455 | TODO | // TODO: derive the factors from the game progress |
| opti_chess/eval_misc.cpp:1457 | TODO | // TODO: implement the rest of the logic for middlegame winning chances |
| opti_chess/eval_misc.cpp:1464 | FIXME | // FIXME: weight this even more, since it rarely reaches 0% or 100%? |
| opti_chess/eval_misc.cpp:1471 | TODO | // TODO: take the uncertainty into account (raising winnable?) |
| opti_chess/eval_misc.cpp:1480 | TODO | // TODO *** win conditions |
| opti_chess/eval_misc.cpp:1504 | TODO | // TODO: adapt this to endgames too, so the king closes on the pawns? same for the knights |
| opti_chess/eval_misc.cpp:1505 | TODO | // TODO: add a variable radius for the centre, to tell whether it is extended and judge isolation better |
| opti_chess/eval_misc.cpp:1612 | TODO | // Damping factor when the piece can capture an enemy one back (TODO) |
| opti_chess/eval_misc.cpp:1742 | TODO | // TODO: can the piece trade itself off? If it can, against an equal or higher value, it is not really trapped |
| opti_chess/eval_misc.cpp:1974 | TODO | // *** TODO: fix this? |

### opti_chess/eval_mobility.cpp (18)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/eval_mobility.cpp:17 | TODO | // TODO: add values for square control by pieces? |
| opti_chess/eval_mobility.cpp:35 | TODO | // TODO: check whether double control matters |
| opti_chess/eval_mobility.cpp:167 | TODO | // TODO: a piece can sometimes fully unpin another one (a bishop unpinning a diagonal) |
| opti_chess/eval_mobility.cpp:174 | TODO | // Values of the pinned pieces (TODO: take the other values from the evaluation?) |
| opti_chess/eval_mobility.cpp:190 | FIXME | constexpr float pressuring_factor = 1.5f; // FIXME: too much? |
| opti_chess/eval_mobility.cpp:303 | TODO | // TODO: trouver formule... |
| opti_chess/eval_mobility.cpp:322 | FIXME | // Stop on an enemy piece of the same type (FIXME: this should key on movement type, not necessarily the same piece) |
| opti_chess/eval_mobility.cpp:337 | TODO | // TODO: add the pressure from the other pieces |
| opti_chess/eval_mobility.cpp:338 | TODO | // TODO: add the natural pressure of the pin when the pinned piece is undefended |
| opti_chess/eval_mobility.cpp:357 | FIXME | // FIXME: a pin should never be worth more than the pinned piece |
| opti_chess/eval_mobility.cpp:662 | FIXME | // FIXME: make this a more linear function? |
| opti_chess/eval_mobility.cpp:737 | FIXME | // FIXME: make this a more linear function? |
| opti_chess/eval_mobility.cpp:862 | TODO | // TODO: make this non-linear? |
| opti_chess/eval_mobility.cpp:887 | TODO | // TODO: how should pinned pieces be handled? |
| opti_chess/eval_mobility.cpp:888 | TODO | // TODO: for a bishop, 7 versus 8 available moves barely differ any more |
| opti_chess/eval_mobility.cpp:1666 | TODO | // TODO (tuning candidate): softer blocking factors — pawn 0.75, |
| opti_chess/eval_mobility.cpp:1668 | TODO | // TODO (tuning candidate): soften the table low end (bishop/queen/knight |
| opti_chess/eval_mobility.cpp:1671 | TODO | // TODO (retry): own-pawn-ahead ray cap (0/1/2 by pawn pushes, captures |

### opti_chess/eval_pawn.cpp (19)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/eval_pawn.cpp:112 | TODO | // TODO: less severe when there is activity |
| opti_chess/eval_pawn.cpp:185 | TODO | // TODO (new component): overextension / V-structure — advanced pawn |
| opti_chess/eval_pawn.cpp:1225 | TODO | // TODO: to favour "A" structures over "V" ones: |
| opti_chess/eval_pawn.cpp:1365 | TODO | // TODO: the per-progress multipliers should be applied at the end |
| opti_chess/eval_pawn.cpp:1382 | TODO | // TODO: improve the control handling; as it stands a bishop is never threatened, even with the pawn protected |
| opti_chess/eval_pawn.cpp:1611 | TODO | // - TODO: semi-open files in front of the king |
| opti_chess/eval_pawn.cpp:1612 | TODO | // - TODO: penalties for doubled or isolated pawns in front of the king |
| opti_chess/eval_pawn.cpp:1613 | TODO | // - open files/diagonals: TODO |
| opti_chess/eval_pawn.cpp:1680 | FIXME | // FIXME: should there be a bonus once the pawn is past the enemy king? That effectively puts the king on an "open" file without it truly be |
| opti_chess/eval_pawn.cpp:1681 | FIXME | // FIXME: does an enemy piece block the pawn storm, or only pawns? |
| opti_chess/eval_pawn.cpp:1741 | FIXME | // FIXME?? |
| opti_chess/eval_pawn.cpp:1761 | TODO | // TODO: handle the uncastled case: estimate the possible post-castling squares (file c or g) and derive the shielding power of the pawn str |
| opti_chess/eval_pawn.cpp:1840 | TODO | // TODO: handle the uncastled case: estimate the possible post-castling squares (file c or g) and derive the shielding power of the pawn str |
| opti_chess/eval_pawn.cpp:1857 | TODO | // TODO: take the distances to castling into account |
| opti_chess/eval_pawn.cpp:1858 | FIXME | // FIXME: not ideal, since only the structure is considered when judging whether castling is a good idea |
| opti_chess/eval_pawn.cpp:1884 | TODO | //// TODO: could be improved when pieces other than the original bishop block the castle |
| opti_chess/eval_pawn.cpp:1894 | FIXME | //// FIXME: fairly arbitrary. 0.9 when castling is available, decaying linearly to 0 otherwise, assuming the distance never exceeds 8 |
| opti_chess/eval_pawn.cpp:1908 | FIXME | //// FIXME: one special case makes this bad: |
| opti_chess/eval_pawn.cpp:2155 | TODO | // TODO *** |

### opti_chess/evaluation.cpp (1)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/evaluation.cpp:115 | TODO | // TODO !!! |

### opti_chess/evaluation.h (1)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/evaluation.h:363 | TODO | // TODO !! |

### opti_chess/exploration.cpp (14)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/exploration.cpp:240 | FIXME | // FIXME: check whether the move is already among the children? |
| opti_chess/exploration.cpp:369 | TODO | // TODO: |
| opti_chess/exploration.cpp:376 | FIXME | // FIXME: this should not happen |
| opti_chess/exploration.cpp:441 | FIXME | // FIXME: this should not happen |
| opti_chess/exploration.cpp:524 | FIXME | // FIXME: this should not happen. Under DAG, _nodes is a per-edge proxy |
| opti_chess/exploration.cpp:699 | TODO | child->quiescence(board_buffer, eval, 2, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &branch_history); // TODO: cut off more eager |
| opti_chess/exploration.cpp:820 | FIXME | // FIXME: what to do when every child has been examined and no move improves the evaluation? |
| opti_chess/exploration.cpp:1172 | TODO | // TODO: improve this sort |
| opti_chess/exploration.cpp:1184 | TODO | // TODO: use the move score as a secondary sort key |
| opti_chess/exploration.cpp:1576 | FIXME | // FIXME: is it right to carry the beta margin down the whole depth? |
| opti_chess/exploration.cpp:1585 | TODO | // TODO: try stand-pat pruning (futility pruning) |
| opti_chess/exploration.cpp:1851 | TODO | // TODO: should only be used to tell whether the buffer is full |
| opti_chess/exploration.cpp:2000 | FIXME | // FIXME *** what to do if there no fully explored move? |
| opti_chess/exploration.cpp:2001 | FIXME | // FIXME *** should we just change the overall score to the potential eval we are getting here instead of a multiplier? |

### opti_chess/exploration.h (5)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/exploration.h:153 | TODO | // TODO: |
| opti_chess/exploration.h:167 | FIXME | // Board: FIXME -> index of the board in the buffer? |
| opti_chess/exploration.h:170 | FIXME | // Move played to reach this board (FIXME: is it already stored in the board?) |
| opti_chess/exploration.h:210 | TODO | // TODO: several node types are needed: quiet nodes (the ones searched by GrogrosZero), quiescence nodes, transposition nodes... |
| opti_chess/exploration.h:359 | TODO | // Destructor: TODO (delete every dynamic array...) |

### opti_chess/exploration_diag.cpp (14)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/exploration_diag.cpp:345 | FIXME | // FIXME: check whether the move is already among the children? |
| opti_chess/exploration_diag.cpp:522 | TODO | // TODO: |
| opti_chess/exploration_diag.cpp:529 | FIXME | // FIXME: this should not happen |
| opti_chess/exploration_diag.cpp:597 | FIXME | // FIXME: this should not happen |
| opti_chess/exploration_diag.cpp:695 | FIXME | // FIXME: this should not happen. Under DAG, _nodes is a per-edge proxy |
| opti_chess/exploration_diag.cpp:1249 | FIXME | // FIXME: what to do when every child has been examined and no move improves the evaluation? |
| opti_chess/exploration_diag.cpp:1906 | TODO | // TODO: improve this sort |
| opti_chess/exploration_diag.cpp:1918 | TODO | // TODO: use the move score as a secondary sort key |
| opti_chess/exploration_diag.cpp:2316 | FIXME | // FIXME: is it right to carry the beta margin down the whole depth? |
| opti_chess/exploration_diag.cpp:2326 | TODO | // TODO: try stand-pat pruning (futility pruning) |
| opti_chess/exploration_diag.cpp:2384 | FIXME | // FIXME: re-enable with a proper cap (e.g. max_depth = initial quiescence_depth) |
| opti_chess/exploration_diag.cpp:2740 | TODO | // TODO: should only be used to tell whether the buffer is full |
| opti_chess/exploration_diag.cpp:2969 | FIXME | // FIXME *** what to do if there no fully explored move? |
| opti_chess/exploration_diag.cpp:2970 | FIXME | // FIXME *** should we just change the overall score to the potential eval we are getting here instead of a multiplier? |

### opti_chess/game_tree.cpp (3)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/game_tree.cpp:27 | FIXME | string display = is_current_node ? "()" : ""; // FIXME: to be improved |
| opti_chess/game_tree.cpp:190 | FIXME | // FIXME: there are still bugs here........ |
| opti_chess/game_tree.cpp:230 | FIXME | // FIXME: this does not work, we are not on the right node |

### opti_chess/game_tree.h (2)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/game_tree.h:26 | TODO | clock_t _time; // TODO |
| opti_chess/game_tree.h:28 | TODO | // TODO: (to add to the PGN) |

### opti_chess/gui.cpp (17)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/gui.cpp:381 | TODO | //	// TODO: |
| opti_chess/gui.cpp:408 | TODO | // TODO |
| opti_chess/gui.cpp:681 | TODO | _icon = LoadImage("resources/images/grogros_zero.png"); // TODO essayer de charger le .ico, pour que l'icone s'affiche tout le temps (pas se |
| opti_chess/gui.cpp:761 | FIXME | // FIXME *** test: if nothing changed in the text or the dimensions, this text could be cached, no? |
| opti_chess/gui.cpp:907 | FIXME | // FIXME: this assumes the evaluation never goes above +100 (or +10000 in Grogros's equivalent) |
| opti_chess/gui.cpp:1039 | FIXME | // FIXME: the real cases to distinguish: |
| opti_chess/gui.cpp:1112 | TODO | // The parent and all the children will have to be deleted (TODO) |
| opti_chess/gui.cpp:2339 | TODO | // TODO |
| opti_chess/gui.cpp:2398 | TODO | // TODO: add a slider value |
| opti_chess/gui.cpp:2424 | TODO | // TODO: the FEN has to be validated |
| opti_chess/gui.cpp:2573 | TODO | // TODO: base this on the number of SEARCH nodes, to spend more time in complex positions |
| opti_chess/gui.cpp:2576 | TODO | // TODO |
| opti_chess/gui.cpp:2691 | FIXME | // FIXME *** the notion of best move needs revisiting here |
| opti_chess/gui.cpp:2705 | FIXME/TODO | // FIXME: check whether this works well (TODO) |
| opti_chess/gui.cpp:2715 | FIXME | // FIXME: things to improve here! |
| opti_chess/gui.cpp:2735 | FIXME | // FIXME: should the evaluation gap be relative? |
| opti_chess/gui.cpp:2793 | FIXME | // Overflow (FIXME: this needs better handling...) |

### opti_chess/gui.h (4)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/gui.h:57 | TODO | // TODO: add const in a lot more places |
| opti_chess/gui.h:150 | FIXME | bool _explore_checks = true; // FIXME? do the checks really have to be explored? |
| opti_chess/gui.h:160 | TODO | // TODO: a player class with name, elo, country, url, title... |
| opti_chess/gui.h:240 | TODO | // TODO: threads (for the parallelization) |

### opti_chess/main_gui.h (3)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/main_gui.h:485 | FIXME | cout << "FIXME BEFORE USING" << endl; |
| opti_chess/main_gui.h:795 | TODO | // TODO: all of this needs improving, and some of it should move into the GUI |
| opti_chess/main_gui.h:833 | TODO | // When it is its turn (TODO: a function for that) |

### opti_chess/main_lichess.h (8)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/main_lichess.h:15 | TODO | // TODO: |
| opti_chess/main_lichess.h:21 | TODO | // Look at the TODOs in the code |
| opti_chess/main_lichess.h:203 | TODO | // TODO: to be checked |
| opti_chess/main_lichess.h:206 | TODO | // TODO: take the rest into account? (movetime...) |
| opti_chess/main_lichess.h:256 | TODO | max_move_time *= (1 + board._adv); // Check whether this works well (TODO) |
| opti_chess/main_lichess.h:265 | FIXME | nodes_to_play = wait_for_best_move ? nodes_to_play : nodes_to_play / 4; // FIXME: the wait could depend on the evaluation gap between the be |
| opti_chess/main_lichess.h:272 | TODO | // TODO: play the node count of the next iteration based on an estimate of the nodes left |
| opti_chess/main_lichess.h:275 | TODO | // TODO: take the increment into account |

### opti_chess/neural_network.h (2)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/neural_network.h:15 | TODO | // TODO: take parameters other than just the pieces into account? (side to move, move count...) |
| opti_chess/neural_network.h:53 | TODO | // TODO |

### opti_chess/tests.cpp (2)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/tests.cpp:64 | TODO | // TODO: wire this straight to the GUI, so the position and the running tests are visible |
| opti_chess/tests.cpp:163 | TODO | // TODO: keep the GUI up to date as the tests go |

### opti_chess/tests.h (2)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/tests.h:10 | TODO | // Evaluation tests on various positions: a score based on how close the evaluation is to the real one, and on the frequency of the position |
| opti_chess/tests.h:28 | TODO | // TODO *** |

### opti_chess/useful_functions.cpp (1)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/useful_functions.cpp:317 | TODO | // TODO: the negative case still has to be handled |

### opti_chess/windows_tests.cpp (2)

| file:line | tag | comment (trimmed, max 140 chars) |
|---|---|---|
| opti_chess/windows_tests.cpp:77 | FIXME | // FIXME: this does not return the right thing |
| opti_chess/windows_tests.cpp:405 | TODO | // TODO: clean |

## B) Open items (not marked fixed/done/closed)

### B1) opti_chess/BUGFIXES.md
- BUGFIXES #11 - Architectural limitation: TT yields no depth gain. Plan A measured insufficient; plan B (DAG) open. Cites exploration.cpp:739, :770/:778/:801/:982/:996, :272-273 - BUGFIXES.md:86 - severity HIGH
- BUGFIXES #9 - 2-fold vs 3-fold repetition. Doc says confirmed intentional by user (BUGFIXES.md:166) but status still to-verify - opti_chess/exploration.cpp:7,29-31 - BUGFIXES.md:123 - severity none given
- BUGFIXES M1 - stand-pat mid-loop maxima mutation. Status ACCEPTED (reverted), "slightly inconsistent but harmless" - opti_chess/exploration_diag.cpp:1998-2010 - BUGFIXES.md:149 - severity none given
- BUGFIXES H4 - best_score init 0.0. Status ACCEPTED (reverted), acts as quality floor - no file:line cited - BUGFIXES.md:159 - severity none given
Excluded as closed: #1-#8, #10, #12-#14, H1-H3, M2, M3. Note: #3 and #14 sit under the "Open" heading but are marked fixed.
### B2) TECH_DEBT.md (root)
- TD-002 - DAG freeze: path-local repetition draws are cut without devaluing the cyclic edge, so a shared value freezes - opti_chess/exploration_diag.cpp:1337 - TECH_DEBT.md:8 - severity high
- TD-003 - Board is 2056 B, pools saturate on 200k-node probes. Doc text is partly superseded by TD-004 (sizeof now 648 B) - opti_chess/board.h (sizeof(Board)) - TECH_DEBT.md:9 - severity medium
- TD-009 - LATENT shared-tree mode (publish/claim/placeholder paths) defaults OFF, no test coverage - opti_chess/exploration_diag.cpp (g_shared_tree paths) - TECH_DEBT.md:10 - severity low
- TD-008 - Run-to-run nondeterminism from unseeded Zobrist keys; probes need OPTI_SEED=42 - no file:line (probes/tests) - TECH_DEBT.md:11 - severity medium
- TD-005 - Quiescence H1/H3 RE-APPLIED, measured and kept; no fixed/done marker in row - opti_chess/exploration_diag.cpp (quiescence H1/H3) - TECH_DEBT.md:14 - severity low
- TD-011 - NEXT DIET (user-approved): shared fixed move buffer, ownership TBD; _moves[100] = 300 B - opti_chess/board.h (_moves[100]) - TECH_DEBT.md:17 - severity medium
- TD-012 - Eval h3 flat: pp_cap caps contested squares, weakest-link min stays on capped own square; defended advancing passer gains nothing - opti_chess/eval_pawn.cpp (pp_cap) - TECH_DEBT.md:18 - severity medium
- TD-013 - "Nonsense best move when buffers full" only partly explained; not reproduced; needs user FEN/log - GUI, no file:line - TECH_DEBT.md:19 - severity high
- TD-014 - NEXT DIET: _array int[8][8] (256 B) to uint8 planned, ownership design pending - opti_chess/board.h (_array int[8][8]) - TECH_DEBT.md:20 - severity medium
- TD-015 - from_fen silently accepts kingless and two-king positions; validation not added - opti_chess/board_fen.cpp (from_fen) - TECH_DEBT.md:21 - severity low
- TD-016 - Marked DONE 08/10 but OPEN residual: live GUI panel check, user confirmation; TT keep-reachable rebuild deferred - GUI recycle/UI/NPS batch, no file:line - TECH_DEBT.md:22 - severity info
Excluded as closed: TD-001, TD-004 (DIET DONE; its accepted risk is noted here: add_move silently drops moves beyond 100, TECH_DEBT.md:15), TD-006, TD-007, TD-010.
### B3) opti_chess/TODO_list.txt (no status markers in file; all items treated as open)
- TODO_list.txt:5 [GAME] Promotions of all types (not only queen) (severity: n/a; no done marker in file)
- TODO_list.txt:6 [GAME] FEN validity tests + make from_fen more tolerant (severity: n/a; no done marker in file)
- TODO_list.txt:7 [GAME] Browse move history (severity: n/a; no done marker in file)
- TODO_list.txt:8 [GAME] Generate an opening tree (severity: n/a; no done marker in file)
- TODO_list.txt:9 [GAME] Opening book (tree format, load/search) (severity: n/a; no done marker in file)
- TODO_list.txt:10 [GAME] Draw/mate rules outside the evaluation (severity: n/a; no done marker in file)
- TODO_list.txt:11 [GAME] Split draw into sub-functions (severity: n/a; no done marker in file)
- TODO_list.txt:12 [GAME] Full move buffer -> handle properly (severity: n/a; no done marker in file)
- TODO_list.txt:15 [EVALUATION] Special endings: king+pawn, bishop+knight mate, opposite-colored bishops, rooks, pawns (severity: n/a; no done marker in file)
- TODO_list.txt:16 [EVALUATION] Winnable: asymmetric (bishop vs knight, minor piece+pawn vs rook) (severity: n/a; no done marker in file)
- TODO_list.txt:17 [EVALUATION] Pawn square in endgames (severity: n/a; no done marker in file)
- TODO_list.txt:18 [EVALUATION] Passed pawns: allied controls, candidates, values (blocker, control, helpers) (severity: n/a; no done marker in file)
- TODO_list.txt:19 [EVALUATION] Backward pawns (severity: n/a; no done marker in file)
- TODO_list.txt:20 [EVALUATION] Pawn structure: islands, chains, "A" vs "V", play on the side the structure points to (severity: n/a; no done marker in file)
- TODO_list.txt:21 [EVALUATION] Favor knights in closed positions, bishops on open diagonals, rooks on open files (severity: n/a; no done marker in file)
- TODO_list.txt:22 [EVALUATION] Weaknesses on one color (severity: n/a; no done marker in file)
- TODO_list.txt:23 [EVALUATION] Light/dark squares -> weakness (severity: n/a; no done marker in file)
- TODO_list.txt:24 [EVALUATION] Weak squares near the king = more serious (group by castling side) (severity: n/a; no done marker in file)
- TODO_list.txt:25 [EVALUATION] Pins: pressure + bonus if the pinned piece is undefended (severity: n/a; no done marker in file)
- TODO_list.txt:26 [EVALUATION] Trapped pieces: do not count if an equivalent piece exchange is possible (severity: n/a; no done marker in file)
- TODO_list.txt:27 [EVALUATION] Enclosed / blocked / isolated pieces (severity: n/a; no done marker in file)
- TODO_list.txt:28 [EVALUATION] Defended / unprotected pieces (solidity) (severity: n/a; no done marker in file)
- TODO_list.txt:29 [EVALUATION] Unprotected pieces (severity: n/a; no done marker in file)
- TODO_list.txt:30 [EVALUATION] King safety: group by castling side (pawn storm, weak squares, diagonals, exposure) (severity: n/a; no done marker in file)
- TODO_list.txt:31 [EVALUATION] Long-term king safety: distances to castling for all factors (severity: n/a; no done marker in file)
- TODO_list.txt:32 [EVALUATION] King proximity: drop vectors (too slow), squares of enemy passed pawns (severity: n/a; no done marker in file)
- TODO_list.txt:33 [EVALUATION] King safety: when own king is safe, OK to weaken it to mate the other (severity: n/a; no done marker in file)
- TODO_list.txt:34 [EVALUATION] Piece activity: distinguish short/long term, pieces blocked by enemy pawns (severity: n/a; no done marker in file)
- TODO_list.txt:35 [EVALUATION] Redo all piece activity (the most important factor) (severity: n/a; no done marker in file)
- TODO_list.txt:36 [EVALUATION] Mobility: map of blocked pieces, iteration, short/long term (severity: n/a; no done marker in file)
- TODO_list.txt:37 [EVALUATION] Piece coordination (which pieces defend each other) (severity: n/a; no done marker in file)
- TODO_list.txt:38 [EVALUATION] Harmonious pieces (severity: n/a; no done marker in file)
- TODO_list.txt:39 [EVALUATION] Pressure: additional x-ray attacks, doubled rooks (severity: n/a; no done marker in file)
- TODO_list.txt:40 [EVALUATION] Initiative / attacking positions (severity: n/a; no done marker in file)
- TODO_list.txt:41 [EVALUATION] Bishop pawns: bonus if in front of the structure, malus if behind (severity: n/a; no done marker in file)
- TODO_list.txt:42 [EVALUATION] Bad bishop = one pawn less (less bad if active) (severity: n/a; no done marker in file)
- TODO_list.txt:43 [EVALUATION] Fianchetto bishops: weakness more serious if not controlled by the bishop (severity: n/a; no done marker in file)
- TODO_list.txt:44 [EVALUATION] Rook behind a passed pawn (severity: n/a; no done marker in file)
- TODO_list.txt:45 [EVALUATION] Connected rooks (severity: n/a; no done marker in file)
- TODO_list.txt:46 [EVALUATION] Keep rooks to draw (severity: n/a; no done marker in file)
- TODO_list.txt:47 [EVALUATION] Queen activity (middlegame/endgame) (severity: n/a; no done marker in file)
- TODO_list.txt:48 [EVALUATION] Queen safety: lower-value pieces only (severity: n/a; no done marker in file)
- TODO_list.txt:49 [EVALUATION] Exposed queen to avoid the Scandinavian (severity: n/a; no done marker in file)
- TODO_list.txt:50 [EVALUATION] Develop knights before bishops (severity: n/a; no done marker in file)
- TODO_list.txt:51 [EVALUATION] Open the position if development gives an advantage (severity: n/a; no done marker in file)
- TODO_list.txt:52 [EVALUATION] Analyze good/bad bishops (severity: n/a; no done marker in file)
- TODO_list.txt:53 [EVALUATION] Mating nets (even without a threatened check) (severity: n/a; no done marker in file)
- TODO_list.txt:54 [EVALUATION] Backrank (severity: n/a; no done marker in file)
- TODO_list.txt:55 [EVALUATION] Overextended space: enemy entry points (severity: n/a; no done marker in file)
- TODO_list.txt:56 [EVALUATION] Defensive potential (bigger minor pieces) (severity: n/a; no done marker in file)
- TODO_list.txt:57 [EVALUATION] Offensive potential of heavy pieces >> defensive heavy pieces (severity: n/a; no done marker in file)
- TODO_list.txt:58 [EVALUATION] Heavy-piece endgames: king advantage, the less weak king (severity: n/a; no done marker in file)
- TODO_list.txt:59 [EVALUATION] Pawn race: count moves before promotion (severity: n/a; no done marker in file)
- TODO_list.txt:60 [EVALUATION] No pawn_push_threat if en passant is possible (severity: n/a; no done marker in file)
- TODO_list.txt:61 [EVALUATION] Incremental evaluation (severity: n/a; no done marker in file)
- TODO_list.txt:62 [EVALUATION] Generate ordered moves (captures first) (severity: n/a; no done marker in file)
- TODO_list.txt:63 [EVALUATION] Increase pawn pushes in endgames (move sort / policy) (severity: n/a; no done marker in file)
- TODO_list.txt:64 [EVALUATION] Move policy (thinking preferences) (severity: n/a; no done marker in file)
- TODO_list.txt:65 [EVALUATION] Two-weaknesses rule (severity: n/a; no done marker in file)
- TODO_list.txt:66 [EVALUATION] Anaconda: controls squares, no opponent move (severity: n/a; no done marker in file)
- TODO_list.txt:67 [EVALUATION] Active/passive positional themes (short/long term) (severity: n/a; no done marker in file)
- TODO_list.txt:68 [EVALUATION] Open lines/files to the king: serious only if a heavy piece can get there (severity: n/a; no done marker in file)
- TODO_list.txt:69 [EVALUATION] "Development" category separate from king safety (severity: n/a; no done marker in file)
- TODO_list.txt:70 [EVALUATION] Review the overall value piece by piece (severity: n/a; no done marker in file)
- TODO_list.txt:71 [EVALUATION] Remove pawn_shield/pawn_storm from eval (already in king_safety) (severity: n/a; no done marker in file)
- TODO_list.txt:72 [EVALUATION] Texel tuner to correct the evaluation (severity: n/a; no done marker in file)
- TODO_list.txt:73 [EVALUATION] Use the control map in the evaluation (severity: n/a; no done marker in file)
- TODO_list.txt:74 [EVALUATION] Draw handling: max uncertainty, winning eval, beta down (severity: n/a; no done marker in file)
- TODO_list.txt:75 [EVALUATION] Compute progress as we go (severity: n/a; no done marker in file)
- TODO_list.txt:76 [EVALUATION] 50-move rule: global variable (severity: n/a; no done marker in file)
- TODO_list.txt:77 [EVALUATION] Split short-term / long-term eval (severity: n/a; no done marker in file)
- TODO_list.txt:78 [EVALUATION] Confidence: 100% if more pieces? (severity: n/a; no done marker in file)
- TODO_list.txt:79 [EVALUATION] Uncertainty: long castling makes positions more uncertain (severity: n/a; no done marker in file)
- TODO_list.txt:80 [EVALUATION] Number of pawn islands in the structure (severity: n/a; no done marker in file)
- TODO_list.txt:81 [EVALUATION] WR must not decrease when activity increases for a given eval (severity: n/a; no done marker in file)
- TODO_list.txt:82 [EVALUATION] In endgames, passed pawns or not, they must be pushed (severity: n/a; no done marker in file)
- TODO_list.txt:83 [EVALUATION] Rework evaluate threat to be more powerful (severity: n/a; no done marker in file)
- TODO_list.txt:84 [EVALUATION] Increase evaluation speed (severity: n/a; no done marker in file)
- TODO_list.txt:87 [ALGORITHM] Negamax algorithm: undo moves instead of copying arrays (severity: n/a; no done marker in file)
- TODO_list.txt:88 [ALGORITHM] MTD(f) <-> alphabeta with memory (severity: n/a; no done marker in file)
- TODO_list.txt:89 [ALGORITHM] Parallelization (GPU?, std::for_each parallel policy, threads) (severity: n/a; no done marker in file)
- TODO_list.txt:90 [ALGORITHM] Parallelize perft tests (severity: n/a; no done marker in file)
- TODO_list.txt:91 [ALGORITHM] SIMD code (severity: n/a; no done marker in file)
- TODO_list.txt:92 [ALGORITHM] Move buffer to avoid storing on boards (severity: n/a; no done marker in file)
- TODO_list.txt:93 [ALGORITHM] Store board index in buffer instead of pointer (severity: n/a; no done marker in file)
- TODO_list.txt:94 [ALGORITHM] Negamax: use boards from the buffer (severity: n/a; no done marker in file)
- TODO_list.txt:95 [ALGORITHM] Precompute (edge distance matrices, constants, attack tables) (severity: n/a; no done marker in file)
- TODO_list.txt:96 [ALGORITHM] Use boost library (severity: n/a; no done marker in file)
- TODO_list.txt:97 [ALGORITHM] Use std::sort to sort moves (severity: n/a; no done marker in file)
- TODO_list.txt:98 [ALGORITHM] Transpositions: reset table after each move (severity: n/a; no done marker in file)
- TODO_list.txt:99 [ALGORITHM] 16-bit mini Zobrist key for repetitions (severity: n/a; no done marker in file)
- TODO_list.txt:100 [ALGORITHM] Delta pruning (quiescence search) (severity: n/a; no done marker in file)
- TODO_list.txt:101 [ALGORITHM] Review k_add (severity: n/a; no done marker in file)
- TODO_list.txt:102 [ALGORITHM] Quiescence: shallow first, deep if interesting (severity: n/a; no done marker in file)
- TODO_list.txt:103 [ALGORITHM] Quiescence: cut positions that are completely lost (severity: n/a; no done marker in file)
- TODO_list.txt:104 [ALGORITHM] Quiescence: reuse thinking for the normal search (severity: n/a; no done marker in file)
- TODO_list.txt:105 [ALGORITHM] Quiescence: depth modulated by branch importance (severity: n/a; no done marker in file)
- TODO_list.txt:106 [ALGORITHM] Quiescence: review standpat permissions (severity: n/a; no done marker in file)
- TODO_list.txt:107 [ALGORITHM] Quiescence: depth < 0 to investigate (severity: n/a; no done marker in file)
- TODO_list.txt:108 [ALGORITHM] Quiescence: move ordering (best captures first) (severity: n/a; no done marker in file)
- TODO_list.txt:109 [ALGORITHM] Stop looking at finished branches of GrogrosZero (severity: n/a; no done marker in file)
- TODO_list.txt:110 [ALGORITHM] Search wide at first, then narrow later (severity: n/a; no done marker in file)
- TODO_list.txt:111 [ALGORITHM] Remove bad moves to increase storage capacity (severity: n/a; no done marker in file)
- TODO_list.txt:112 [ALGORITHM] Prioritize moves not fully explored (severity: n/a; no done marker in file)
- TODO_list.txt:113 [ALGORITHM] Transpositions: handle overflows (severity: n/a; no done marker in file)
- TODO_list.txt:114 [ALGORITHM] Beta cut-off if in check? (severity: n/a; no done marker in file)
- TODO_list.txt:115 [ALGORITHM] Smaller q_depth with extensions (severity: n/a; no done marker in file)
- TODO_list.txt:116 [ALGORITHM] positions_history in the search rather than board (severity: n/a; no done marker in file)
- TODO_list.txt:117 [ALGORITHM] Pick_random_child based on relative evaluation (severity: n/a; no done marker in file)
- TODO_list.txt:118 [ALGORITHM] When the best move is fully explored, redo without it? (severity: n/a; no done marker in file)
- TODO_list.txt:119 [ALGORITHM] Use Visual Studio improvements/optimizations (severity: n/a; no done marker in file)
- TODO_list.txt:120 [ALGORITHM] GrogrosZero slows long variations -> optimize (severity: n/a; no done marker in file)
- TODO_list.txt:121 [ALGORITHM] Search specifically for mate (severity: n/a; no done marker in file)
- TODO_list.txt:122 [ALGORITHM] Raylib for random (check speed) (severity: n/a; no done marker in file)
- TODO_list.txt:123 [ALGORITHM] Initialize board instead of from_fen (severity: n/a; no done marker in file)
- TODO_list.txt:124 [ALGORITHM] Undo keeps GrogrosZero calculations (severity: n/a; no done marker in file)
- TODO_list.txt:125 [ALGORITHM] Redo get_moves (controlled squares) + perft test (severity: n/a; no done marker in file)
- TODO_list.txt:126 [ALGORITHM] Cleanup: move flags, useful methods, static constexpr (severity: n/a; no done marker in file)
- TODO_list.txt:127 [ALGORITHM] Redo get_best_move to match real values (WDL) (severity: n/a; no done marker in file)
- TODO_list.txt:128 [ALGORITHM] Reduce quiescence depth for bad moves (severity: n/a; no done marker in file)
- TODO_list.txt:129 [ALGORITHM] Handle variant promotions (whole branch + selecting the right node) (severity: n/a; no done marker in file)
- TODO_list.txt:130 [ALGORITHM] Implement new quiescence passes (severity: n/a; no done marker in file)
- TODO_list.txt:133 [INTERFACE] Readme for the controls (severity: n/a; no done marker in file)
- TODO_list.txt:134 [INTERFACE] Sounds: improvement, homemade, time, ambience, "soft" (severity: n/a; no done marker in file)
- TODO_list.txt:135 [INTERFACE] Images/new textures (severity: n/a; no done marker in file)
- TODO_list.txt:136 [INTERFACE] Options: disable sound, menus, AI search settings (severity: n/a; no done marker in file)
- TODO_list.txt:137 [INTERFACE] Position editor (add/remove pieces) (severity: n/a; no done marker in file)
- TODO_list.txt:138 [INTERFACE] Dedicated display thread (PARALLELIZE, huge lag) (severity: n/a; no done marker in file)
- TODO_list.txt:139 [INTERFACE] Undo removes the move from the PGN (severity: n/a; no done marker in file)
- TODO_list.txt:140 [INTERFACE] Different text colors, more contrast (severity: n/a; no done marker in file)
- TODO_list.txt:141 [INTERFACE] Scroll variation on mouse hover (severity: n/a; no done marker in file)
- TODO_list.txt:142 [INTERFACE] Show mate/stalemate on the board (severity: n/a; no done marker in file)
- TODO_list.txt:143 [INTERFACE] Unload images/textures to free RAM (severity: n/a; no done marker in file)
- TODO_list.txt:144 [INTERFACE] Change AI parameters in the UI (severity: n/a; no done marker in file)
- TODO_list.txt:145 [INTERFACE] Save full games to a file (severity: n/a; no done marker in file)
- TODO_list.txt:146 [INTERFACE] MC analyses: show the best-eval path and the played path (severity: n/a; no done marker in file)
- TODO_list.txt:147 [INTERFACE] Grab slider / click to change position (severity: n/a; no done marker in file)
- TODO_list.txt:148 [INTERFACE] Battles between evaluation parameters (severity: n/a; no done marker in file)
- TODO_list.txt:149 [INTERFACE] Import/export PGN (severity: n/a; no done marker in file)
- TODO_list.txt:150 [INTERFACE] Show Grogros thinking on the PGN (severity: n/a; no done marker in file)
- TODO_list.txt:151 [INTERFACE] Change node count / depth in the GUI (severity: n/a; no done marker in file)
- TODO_list.txt:152 [INTERFACE] Smooth evaluation bar (severity: n/a; no done marker in file)
- TODO_list.txt:153 [INTERFACE] Evaluation graph at end of game (severity: n/a; no done marker in file)
- TODO_list.txt:154 [INTERFACE] Logarithmic scale for eval bar (severity: n/a; no done marker in file)
- TODO_list.txt:155 [INTERFACE] Fix time management (displayed != real) (severity: n/a; no done marker in file)
- TODO_list.txt:156 [INTERFACE] Clean GUI implementation (severity: n/a; no done marker in file)
- TODO_list.txt:157 [INTERFACE] Pre-moves from scratch (+ multiple) (severity: n/a; no done marker in file)
- TODO_list.txt:158 [INTERFACE] Arrows: computed variation, main moves (severity: n/a; no done marker in file)
- TODO_list.txt:159 [INTERFACE] Show the threat (change the turn) (severity: n/a; no done marker in file)
- TODO_list.txt:160 [INTERFACE] Grogros animations that move the pieces (severity: n/a; no done marker in file)
- TODO_list.txt:161 [INTERFACE] Deselect piece after move played (severity: n/a; no done marker in file)
- TODO_list.txt:162 [INTERFACE] Int overflow in softmax (severity: n/a; no done marker in file)
- TODO_list.txt:163 [INTERFACE] Display based on the game tree (severity: n/a; no done marker in file)
- TODO_list.txt:164 [INTERFACE] Time management based on the difference between best and other moves (severity: n/a; no done marker in file)
- TODO_list.txt:165 [INTERFACE] Navigate variation: click, change, unhighlight (severity: n/a; no done marker in file)
- TODO_list.txt:166 [INTERFACE] From_fen -> reset the tree (severity: n/a; no done marker in file)
- TODO_list.txt:167 [INTERFACE] Keep GrogrosZero thinking while navigating (severity: n/a; no done marker in file)
- TODO_list.txt:168 [INTERFACE] Promote variation to main line (severity: n/a; no done marker in file)
- TODO_list.txt:169 [INTERFACE] Show rising/falling line (severity: n/a; no done marker in file)
- TODO_list.txt:170 [INTERFACE] Sounds when navigating (severity: n/a; no done marker in file)
- TODO_list.txt:171 [INTERFACE] Show strengths/weaknesses (green/orange/red) (severity: n/a; no done marker in file)
- TODO_list.txt:172 [INTERFACE] Show current move in variation (severity: n/a; no done marker in file)
- TODO_list.txt:173 [INTERFACE] Variation with details (depth) (severity: n/a; no done marker in file)
- TODO_list.txt:174 [INTERFACE] Time + nodes + eval in PGN (severity: n/a; no done marker in file)
- TODO_list.txt:175 [INTERFACE] Improved visual UTF-8 pieces (severity: n/a; no done marker in file)
- TODO_list.txt:176 [INTERFACE] Color squares by piece bonus (severity: n/a; no done marker in file)
- TODO_list.txt:177 [INTERFACE] FEN auto-completion if incorrect (severity: n/a; no done marker in file)
- TODO_list.txt:178 [INTERFACE] Show emergency cutoff info (severity: n/a; no done marker in file)
- TODO_list.txt:179 [INTERFACE] Analysis mode black POV (bug) (severity: n/a; no done marker in file)
- TODO_list.txt:180 [INTERFACE] Show binding site (severity: n/a; no done marker in file)
- TODO_list.txt:181 [INTERFACE] Cancel bindings (CTRL+Y) (severity: n/a; no done marker in file)
- TODO_list.txt:182 [INTERFACE] Version number in UI (severity: n/a; no done marker in file)
- TODO_list.txt:183 [INTERFACE] Scroll variation with held arrow (severity: n/a; no done marker in file)
- TODO_list.txt:184 [INTERFACE] GPU textures + shaders (outline, non-blurry text) (severity: n/a; no done marker in file)
- TODO_list.txt:185 [INTERFACE] UI bug 12 FPS -> fix (severity: n/a; no done marker in file)
- TODO_list.txt:186 [INTERFACE] Bezier lines/circles not nice -> improve (severity: n/a; no done marker in file)
- TODO_list.txt:189 [INFRASTRUCTURE] Split board.cpp into dedicated files (GUI, AI, evaluation) (severity: n/a; no done marker in file)
- TODO_list.txt:190 [INFRASTRUCTURE] Make a class for the AIs (severity: n/a; no done marker in file)
- TODO_list.txt:191 [INFRASTRUCTURE] Which compiler is fastest/best optimized (severity: n/a; no done marker in file)
- TODO_list.txt:192 [INFRASTRUCTURE] Switch comments to English + clean code (severity: n/a; no done marker in file)
- TODO_list.txt:193 [INFRASTRUCTURE] Version names for Grogros (severity: n/a; no done marker in file)
- TODO_list.txt:194 [INFRASTRUCTURE] Unit tests for functions (severity: n/a; no done marker in file)
- TODO_list.txt:195 [INFRASTRUCTURE] Micro tests of the algorithm (efficiency at minimal time) (severity: n/a; no done marker in file)
- TODO_list.txt:196 [INFRASTRUCTURE] Full documentation + refactoring (severity: n/a; no done marker in file)
- TODO_list.txt:197 [INFRASTRUCTURE] Clean parallelization (severity: n/a; no done marker in file)
- TODO_list.txt:198 [INFRASTRUCTURE] Profiler for slow functions (severity: n/a; no done marker in file)
- TODO_list.txt:199 [INFRASTRUCTURE] Clean up exploration code (severity: n/a; no done marker in file)
- TODO_list.txt:200 [INFRASTRUCTURE] Global cleanup (useless comments, factorize, micro-optimize) (severity: n/a; no done marker in file)
- TODO_list.txt:201 [INFRASTRUCTURE] Optimize evaluation (severity: n/a; no done marker in file)
- TODO_list.txt:202 [INFRASTRUCTURE] Test framework (severity: n/a; no done marker in file)
- TODO_list.txt:203 [INFRASTRUCTURE] Evaluation speed test (severity: n/a; no done marker in file)
- TODO_list.txt:204 [INFRASTRUCTURE] Test function: compare Grogros vs Leela evals (severity: n/a; no done marker in file)
- TODO_list.txt:205 [INFRASTRUCTURE] Download positions for tests (severity: n/a; no done marker in file)
- TODO_list.txt:206 [INFRASTRUCTURE] Track memory leaks (severity: n/a; no done marker in file)
- TODO_list.txt:207 [INFRASTRUCTURE] robin_maps instead of maps (severity: n/a; no done marker in file)
- TODO_list.txt:208 [INFRASTRUCTURE] Screenshots in wrong folder -> fix (severity: n/a; no done marker in file)
- TODO_list.txt:209 [INFRASTRUCTURE] Crash when closing the window (severity: n/a; no done marker in file)
- TODO_list.txt:212 [NEURAL NETWORKS] Matches with zero thinking, network only (severity: n/a; no done marker in file)
- TODO_list.txt:213 [NEURAL NETWORKS] Symmetry in the network? (severity: n/a; no done marker in file)
- TODO_list.txt:214 [NEURAL NETWORKS] Learning: how do we do it? (severity: n/a; no done marker in file)
- TODO_list.txt:215 [NEURAL NETWORKS] Self-play games (severity: n/a; no done marker in file)
- TODO_list.txt:216 [NEURAL NETWORKS] Genetic algorithm (destroy weak ones, strong ones mutate) (severity: n/a; no done marker in file)
- TODO_list.txt:217 [NEURAL NETWORKS] Bonus/malus when it evaluated badly (severity: n/a; no done marker in file)
- TODO_list.txt:218 [NEURAL NETWORKS] End-game learning, then work back up (severity: n/a; no done marker in file)
- TODO_list.txt:219 [NEURAL NETWORKS] Reinforcement learning (severity: n/a; no done marker in file)
- TODO_list.txt:220 [NEURAL NETWORKS] Parameters beyond pieces (turn, castling) (severity: n/a; no done marker in file)
- TODO_list.txt:221 [NEURAL NETWORKS] Few negative values in evals: normal? (severity: n/a; no done marker in file)
- TODO_list.txt:224 [BUGS] #1 Time: sometimes almost runs out (iterations per s vs nodes per s) (severity: n/a; no done marker in file)
- TODO_list.txt:225 [BUGS] #2 Variation depth to verify (severity: n/a; no done marker in file)
- TODO_list.txt:226 [BUGS] #3 After repetition, Grogros no longer plays (severity: n/a; no done marker in file)
- TODO_list.txt:227 [BUGS] #4 Variation promotions to review (severity: n/a; no done marker in file)
- TODO_list.txt:228 [BUGS] #5 Same move twice in different variations = same parent -> backtracks to the wrong place (severity: n/a; no done marker in file)
- TODO_list.txt:229 [BUGS] #6 Several mate evals -> plays slowly (close evals) (severity: n/a; no done marker in file)
- TODO_list.txt:230 [BUGS] #7 Sometimes game reset (severity: n/a; no done marker in file)
- TODO_list.txt:231 [BUGS] #8 c3: must take h8 (severity: n/a; no done marker in file)
- TODO_list.txt:232 [BUGS] #9 Eval = ±INT_MAX although that is not normal (severity: n/a; no done marker in file)
- TODO_list.txt:233 [BUGS] #10 PGN slider buggy (severity: n/a; no done marker in file)
- TODO_list.txt:234 [BUGS] #11 Buffer sometimes full anyway (severity: n/a; no done marker in file)
- TODO_list.txt:235 [BUGS] #12 Transpositions: thinks it is a win when it is a draw + click returns a corrupted position (6k1/8/7P/7K/8/8/8/8 w - - 3 72) (severity: n/a; no done marker in file)
- TODO_list.txt:236 [BUGS] #13 Transpositions do not increase depth in endgame (severity: n/a; no done marker in file)
- TODO_list.txt:237 [BUGS] #14 8/6pk/7p/8/1bQB3P/p1p3P1/5P1K/1q6 w - -: thinks Df7 wins (severity: n/a; no done marker in file)
- TODO_list.txt:238 [BUGS] #15 Backtrack crash (severity: n/a; no done marker in file)
- TODO_list.txt:239 [BUGS] #16 Transposition table full -> strange behavior (8/2Q3pk/7p/8/1b1B3P/p1p3P1/5P1K/1q6 b - -: Ff8->0%) (severity: n/a; no done marker in file)
- TODO_list.txt:240 [BUGS] #17 bxa5 not considered (r3k1nr/ppp2ppp/1b1p1q2/n3p3/1PB1P1b1/2P2N2/3P1PPP/RNBQ1RK1 w kq - 0 9) (severity: n/a; no done marker in file)
- TODO_list.txt:241 [BUGS] #18 Transposition temporarily sets -M4 on a move with no mate (2Q5/4b1pk/7p/7P/3B4/p1p3P1/5P1K/1q6 b - -) (severity: n/a; no done marker in file)
- TODO_list.txt:242 [BUGS] #19 Fa6 -M4 then +M4 confidence 87% (r1b3k1/p4ppp/5n2/1Rp4q/4B3/2P5/P1PQ2PP/4R1K1 b - - 3 21) (severity: n/a; no done marker in file)
- TODO_list.txt:243 [BUGS] #20 Transpositions: test (8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7) (severity: n/a; no done marker in file)
- TODO_list.txt:244 [BUGS] #21 Color oscillates blue/magenta on repetition (severity: n/a; no done marker in file)
- TODO_list.txt:245 [BUGS] #22 Sometimes crash (severity: n/a; no done marker in file)
- TODO_list.txt:246 [BUGS] #23 Time stops after repetition (not always) (severity: n/a; no done marker in file)
- TODO_list.txt:247 [BUGS] #24 Piece with no moves: serious if permanent (severity: n/a; no done marker in file)
- TODO_list.txt:248 [BUGS] #25 Crash again (severity: n/a; no done marker in file)
- TODO_list.txt:249 [BUGS] #26 Positions evaluated several times (severity: n/a; no done marker in file)
- TODO_list.txt:250 [BUGS] #30 Quiescence: Fd3 then Fxc3 = bad quiescence (severity: n/a; no done marker in file)
- TODO_list.txt:251 [BUGS] #31 Depth 1 everywhere sometimes (severity: n/a; no done marker in file)
- TODO_list.txt:252 [BUGS] #32 from_fen bug after thinking (severity: n/a; no done marker in file)
- TODO_list.txt:253 [BUGS] #33 Bug crashing the UI (severity: n/a; no done marker in file)
- TODO_list.txt:254 [BUGS] #34 is_stand_pat_eval commented out in quiescence (severity: n/a; no done marker in file)
- TODO_list.txt:255 [BUGS] #35 TODO/REVIEW/FIXME in the middle of the code (severity: n/a; no done marker in file)
- TODO_list.txt:256 [BUGS] #36 The computer crashes when switching to another window (severity: n/a; no done marker in file)
- TODO_list.txt:257 [BUGS] #37 History still totally broken (severity: n/a; no done marker in file)
- TODO_list.txt:258 [BUGS] #38 Thinking time incorrect (severity: n/a; no done marker in file)
- TODO_list.txt:259 [BUGS] #39 Transposed variation: only one parent updated (severity: n/a; no done marker in file)
- TODO_list.txt:260 [BUGS] #40 Alpha-beta pruning in quiescence search to fix (severity: n/a; no done marker in file)
- TODO_list.txt:261 [BUGS] #41 Quiescence checks many times in king+rook endings (very slow) (severity: n/a; no done marker in file)
- TODO_list.txt:262 [BUGS] #42 After playing moves + thinking from another point != same result (severity: n/a; no done marker in file)
- TODO_list.txt:263 [BUGS] #43 Repet: _can_explore false not checked (same for full mates/branches) (severity: n/a; no done marker in file)
- TODO_list.txt:264 [BUGS] #44 12 FPS UI when grogros is idle + piece selection: legal moves not shown (severity: n/a; no done marker in file)

### B4) ROADMAP.md (A1-A4, items 1-3 and 10 excluded as DONE)
- ROADMAP A5 - 10,2,2 and 10,6,2 vs 10,2,0 re-gate: decision recorded (default 10,6,2), no done marker - no file:line - ROADMAP.md:37 - severity n/a
- ROADMAP #229 - hard 6-ply quiet sequence (Qb3 missed), DEPRIORITIZED - no file:line - ROADMAP.md:213-218 - severity n/a
- ROADMAP convergence - "real rates": needs about 4x iterations per 0.1 s (NPS, parallel, or NN ordering) - no file:line - ROADMAP.md:232-245 - severity n/a
- ROADMAP plan step 1 - quiescence node waste (8.8x) to trim; king-safety cache removal "to decide" - ROADMAP.md:247-258 - severity n/a
- ROADMAP plan steps 2-4 - hotpath profiling, parallelization, NN ordering (open) - ROADMAP.md:266-268 - severity n/a
- ROADMAP Goal 1M NPS - target, open - ROADMAP.md:270-273 - severity n/a
- ROADMAP Phase 6 shared tree (Lazy SMP) - conditional next step "if go" - ROADMAP.md:285-288 - severity n/a
- ROADMAP 1.3 Move ordering improvements (TT move, killers, history, countermove) - ROADMAP.md:306 - severity n/a
- ROADMAP 2.1 Selective deepening refinement - ROADMAP.md:326 - severity n/a
- ROADMAP 2.2 Quiescence optimisation - ROADMAP.md:331 - severity n/a
- ROADMAP 2.3 Main search pruning (NMP, LMR, futility, razoring) - ROADMAP.md:337 - severity n/a
- ROADMAP Phase 3 TODO - 5 unchecked boxes (TT main search test, DAG test, node savings, bugs, TT works) - ROADMAP.md:349-354 - severity n/a
- ROADMAP Phase 4 - neural network evaluation (LAST) - ROADMAP.md:362 - severity n/a
- ROADMAP Phase 5 - UI tunability ("next") - ROADMAP.md:397 - severity n/a
- ROADMAP Phase 6 - parallelization - ROADMAP.md:403 - severity n/a
- ROADMAP Phase 7 - NPS optimisation - ROADMAP.md:435 - severity n/a
- ROADMAP priority 1 (marked DONE) - remaining: reproduce random-position corruption, identify root cause - ROADMAP.md:483 - severity n/a
- ROADMAP priority 4 - selective deepening re-evaluation - ROADMAP.md:496 - severity n/a
- ROADMAP priority 5 - full algorithmic audit, focus on correctness bugs - ROADMAP.md:502 - severity n/a
- ROADMAP priority 6 - node wastage optimisation - ROADMAP.md:507 - severity n/a
- ROADMAP priority 7 - TT + DAG audit (check TT storage/lookup mistakes) - ROADMAP.md:512 - severity n/a
- ROADMAP priority 8 - CPU hotpath and RAM - ROADMAP.md:517 - severity n/a
- ROADMAP priority 9 - parallelization - ROADMAP.md:522 - severity n/a
- ROADMAP success criteria - 9 unchecked boxes - ROADMAP.md:544-552 - severity n/a
