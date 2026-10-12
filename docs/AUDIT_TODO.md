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
- TODO_list.txt:5 [JEU] Promotions de tous types (pas seulement dame) (severity: n/a; no done marker in file)
- TODO_list.txt:6 [JEU] Tests de validité de FEN + rendre from_fen plus tolérant (severity: n/a; no done marker in file)
- TODO_list.txt:7 [JEU] Naviguer dans l'historique des coups (severity: n/a; no done marker in file)
- TODO_list.txt:8 [JEU] Générer un arbre d'ouvertures (severity: n/a; no done marker in file)
- TODO_list.txt:9 [JEU] Livre d'ouvertures (format arbre, charger/rechercher) (severity: n/a; no done marker in file)
- TODO_list.txt:10 [JEU] Règles de nulles/mat en dehors de l'évaluation (severity: n/a; no done marker in file)
- TODO_list.txt:11 [JEU] Découper draw en sous-fonctions (severity: n/a; no done marker in file)
- TODO_list.txt:12 [JEU] Buffer de coups plein -> gérer proprement (severity: n/a; no done marker in file)
- TODO_list.txt:15 [EVALUATION] Finales spéciales: roi+pion, mat fou+cavalier, fous couleurs opposées, tours, pions (severity: n/a; no done marker in file)
- TODO_list.txt:16 [EVALUATION] Winnable: asymétriques (fou vs cavalier, pièce mineure+pion vs tour) (severity: n/a; no done marker in file)
- TODO_list.txt:17 [EVALUATION] Carré du pion en finales (severity: n/a; no done marker in file)
- TODO_list.txt:18 [EVALUATION] Pions passés: contrôles alliés, candidats, valeurs (bloqueur, contrôle, aides) (severity: n/a; no done marker in file)
- TODO_list.txt:19 [EVALUATION] Pions arriérés (severity: n/a; no done marker in file)
- TODO_list.txt:20 [EVALUATION] Structure de pions: îlots, chaînes, "A" vs "V", jouer côté où structure pointe (severity: n/a; no done marker in file)
- TODO_list.txt:21 [EVALUATION] Favoriser cavaliers en positions fermées, fous diagonales ouvertes, tours colonnes ouvertes (severity: n/a; no done marker in file)
- TODO_list.txt:22 [EVALUATION] Faiblesses sur une couleur (severity: n/a; no done marker in file)
- TODO_list.txt:23 [EVALUATION] Cases noires/blanches -> faiblesse (severity: n/a; no done marker in file)
- TODO_list.txt:24 [EVALUATION] Cases faibles proches du roi = plus graves (grouper par roque) (severity: n/a; no done marker in file)
- TODO_list.txt:25 [EVALUATION] Clouages: pression + bonus si pièce clouée non défendue (severity: n/a; no done marker in file)
- TODO_list.txt:26 [EVALUATION] Trapped pieces: pas considerer si échange possible pièce équivalente (severity: n/a; no done marker in file)
- TODO_list.txt:27 [EVALUATION] Pièces enfermées / bloquées / isolées (severity: n/a; no done marker in file)
- TODO_list.txt:28 [EVALUATION] Pièces défendues / non protégées (solidité) (severity: n/a; no done marker in file)
- TODO_list.txt:29 [EVALUATION] Unprotected pieces (severity: n/a; no done marker in file)
- TODO_list.txt:30 [EVALUATION] King safety: regrouper par roque (pawn storm, weak squares, diags, exposure) (severity: n/a; no done marker in file)
- TODO_list.txt:31 [EVALUATION] King safety long terme: distances au roque pour tous les facteurs (severity: n/a; no done marker in file)
- TODO_list.txt:32 [EVALUATION] King proximity: virer vecteurs (trop lent), carrés des pions passés adverses (severity: n/a; no done marker in file)
- TODO_list.txt:33 [EVALUATION] King safety: quand roi safe, ok affaiblir le sien pour mater l'autre (severity: n/a; no done marker in file)
- TODO_list.txt:34 [EVALUATION] Activité des pièces: différencier court/long terme, pièces bloquées par pions adverses (severity: n/a; no done marker in file)
- TODO_list.txt:35 [EVALUATION] Refaire toute l'activité des pièces (le facteur le plus important) (severity: n/a; no done marker in file)
- TODO_list.txt:36 [EVALUATION] Mobility: map des pièces bloquées, itération, court/long terme (severity: n/a; no done marker in file)
- TODO_list.txt:37 [EVALUATION] Coordination des pièces (qui se défendent entre elles) (severity: n/a; no done marker in file)
- TODO_list.txt:38 [EVALUATION] Pièces harmonieuses (severity: n/a; no done marker in file)
- TODO_list.txt:39 [EVALUATION] Pression: attaques additionnelles en rayon X, doubler tours (severity: n/a; no done marker in file)
- TODO_list.txt:40 [EVALUATION] Initiative / positions d'attaque (severity: n/a; no done marker in file)
- TODO_list.txt:41 [EVALUATION] Bishop pawns: bonus si devant structure, malus si derrière (severity: n/a; no done marker in file)
- TODO_list.txt:42 [EVALUATION] Mauvais fou = pion de moins (moins pire si actif) (severity: n/a; no done marker in file)
- TODO_list.txt:43 [EVALUATION] Fous de fianchetto: faiblesse plus grave si pas contrôlé par fou (severity: n/a; no done marker in file)
- TODO_list.txt:44 [EVALUATION] Tour derrière pion passé (severity: n/a; no done marker in file)
- TODO_list.txt:45 [EVALUATION] Tours liées (severity: n/a; no done marker in file)
- TODO_list.txt:46 [EVALUATION] Garder les tours pour faire nulle (severity: n/a; no done marker in file)
- TODO_list.txt:47 [EVALUATION] Activité de la dame (milieux/fins de partie) (severity: n/a; no done marker in file)
- TODO_list.txt:48 [EVALUATION] Queen safety: pièces de valeur inférieure seulement (severity: n/a; no done marker in file)
- TODO_list.txt:49 [EVALUATION] Exposed queen pour éviter scandi (severity: n/a; no done marker in file)
- TODO_list.txt:50 [EVALUATION] Développer cavaliers avant fous (severity: n/a; no done marker in file)
- TODO_list.txt:51 [EVALUATION] Ouvrir position si avantage de développement (severity: n/a; no done marker in file)
- TODO_list.txt:52 [EVALUATION] Analyser les bons/mauvais fous (severity: n/a; no done marker in file)
- TODO_list.txt:53 [EVALUATION] Mating nets (même sans échec menacé) (severity: n/a; no done marker in file)
- TODO_list.txt:54 [EVALUATION] Backrank (severity: n/a; no done marker in file)
- TODO_list.txt:55 [EVALUATION] Overextended space: points d'entrée adverses (severity: n/a; no done marker in file)
- TODO_list.txt:56 [EVALUATION] Potentiel défensif (plus gros pièces mineures) (severity: n/a; no done marker in file)
- TODO_list.txt:57 [EVALUATION] Potentiel offensif lourds >> défensif lourds (severity: n/a; no done marker in file)
- TODO_list.txt:58 [EVALUATION] Finales de pièces lourdes: avantage roi le moins faible (severity: n/a; no done marker in file)
- TODO_list.txt:59 [EVALUATION] Pawn race: compter coups avant promotion (severity: n/a; no done marker in file)
- TODO_list.txt:60 [EVALUATION] Pas de pawn_push_threat si en_passant possible (severity: n/a; no done marker in file)
- TODO_list.txt:61 [EVALUATION] Incremental evaluation (severity: n/a; no done marker in file)
- TODO_list.txt:62 [EVALUATION] Générer coups ordonnés (captures en premier) (severity: n/a; no done marker in file)
- TODO_list.txt:63 [EVALUATION] Augmenter poussées pions en finale (move sort / policy) (severity: n/a; no done marker in file)
- TODO_list.txt:64 [EVALUATION] Policy pour les coups (préférences de réflexion) (severity: n/a; no done marker in file)
- TODO_list.txt:65 [EVALUATION] Règle des 2 faiblesses (severity: n/a; no done marker in file)
- TODO_list.txt:66 [EVALUATION] Anaconda: contrôle cases, aucun mouvement adversaire (severity: n/a; no done marker in file)
- TODO_list.txt:67 [EVALUATION] Themes positionnels actifs/passifs (court/long terme) (severity: n/a; no done marker in file)
- TODO_list.txt:68 [EVALUATION] Lignes/colonnes ouvertes sur le roi: grave seulement si pièce lourde peut y aller (severity: n/a; no done marker in file)
- TODO_list.txt:69 [EVALUATION] Catégorie "Développement" séparée de King safety (severity: n/a; no done marker in file)
- TODO_list.txt:70 [EVALUATION] Revoir pièce par pièce la valeur globale (severity: n/a; no done marker in file)
- TODO_list.txt:71 [EVALUATION] Retirer pawn_shield/pawn_storm des éval (déjà dans king_safety) (severity: n/a; no done marker in file)
- TODO_list.txt:72 [EVALUATION] Texel tuner pour rectifier l'évaluation (severity: n/a; no done marker in file)
- TODO_list.txt:73 [EVALUATION] Utiliser map de contrôles dans l'évaluation (severity: n/a; no done marker in file)
- TODO_list.txt:74 [EVALUATION] Gestion des draws: incertitude max, winning eval, beta down (severity: n/a; no done marker in file)
- TODO_list.txt:75 [EVALUATION] Calculer l'avancement au fur et à mesure (severity: n/a; no done marker in file)
- TODO_list.txt:76 [EVALUATION] Règle des 50 coups: variable globale (severity: n/a; no done marker in file)
- TODO_list.txt:77 [EVALUATION] Séparer éval court terme / long terme (severity: n/a; no done marker in file)
- TODO_list.txt:78 [EVALUATION] Confiance: 100% si plus de pièces? (severity: n/a; no done marker in file)
- TODO_list.txt:79 [EVALUATION] Incertitude: grand roque rend positions plus incertaines (severity: n/a; no done marker in file)
- TODO_list.txt:80 [EVALUATION] Nombre d'îlots de pions dans structure (severity: n/a; no done marker in file)
- TODO_list.txt:81 [EVALUATION] WR ne doit diminuer quand activité augmente pour éval donnée (severity: n/a; no done marker in file)
- TODO_list.txt:82 [EVALUATION] En finale, pions passés ou non, faut les pousser (severity: n/a; no done marker in file)
- TODO_list.txt:83 [EVALUATION] Refaire evaluate threat plus puissant (severity: n/a; no done marker in file)
- TODO_list.txt:84 [EVALUATION] Augmenter vitesse d'évaluation (severity: n/a; no done marker in file)
- TODO_list.txt:87 [ALGORITHME] Algo Negamax: undo moves plutôt que copies de tableaux (severity: n/a; no done marker in file)
- TODO_list.txt:88 [ALGORITHME] MTD(f) <-> alphabeta with memory (severity: n/a; no done marker in file)
- TODO_list.txt:89 [ALGORITHME] Parallélisation (GPU?, std::for_each policy parallel, threads) (severity: n/a; no done marker in file)
- TODO_list.txt:90 [ALGORITHME] Paralléliser les perft tests (severity: n/a; no done marker in file)
- TODO_list.txt:91 [ALGORITHME] SIMD code (severity: n/a; no done marker in file)
- TODO_list.txt:92 [ALGORITHME] Buffer de coups pour éviter stockage dans plateaux (severity: n/a; no done marker in file)
- TODO_list.txt:93 [ALGORITHME] Stocker indice plateau dans buffer plutôt que pointeur (severity: n/a; no done marker in file)
- TODO_list.txt:94 [ALGORITHME] Negamax: utiliser plateaux du buffer (severity: n/a; no done marker in file)
- TODO_list.txt:95 [ALGORITHME] Précalculer (matrices distance bords, constantes, tables d'attaque) (severity: n/a; no done marker in file)
- TODO_list.txt:96 [ALGORITHME] Utiliser librairie boost (severity: n/a; no done marker in file)
- TODO_list.txt:97 [ALGORITHME] Utiliser std::sort pour trier les coups (severity: n/a; no done marker in file)
- TODO_list.txt:98 [ALGORITHME] Transpositions: reset table après chaque coup (severity: n/a; no done marker in file)
- TODO_list.txt:99 [ALGORITHME] Mini clé Zobrist 16 bits pour répétitions (severity: n/a; no done marker in file)
- TODO_list.txt:100 [ALGORITHME] Delta pruning (quiescence search) (severity: n/a; no done marker in file)
- TODO_list.txt:101 [ALGORITHME] Revoir k_add (severity: n/a; no done marker in file)
- TODO_list.txt:102 [ALGORITHME] Quiescence: shallow d'abord, profonde si intéressant (severity: n/a; no done marker in file)
- TODO_list.txt:103 [ALGORITHME] Quiescence: couper positions complètement foutues (severity: n/a; no done marker in file)
- TODO_list.txt:104 [ALGORITHME] Quiescence: réutiliser réflexion pour recherche normale (severity: n/a; no done marker in file)
- TODO_list.txt:105 [ALGORITHME] Quiescence: profondeur modulée par importance de la branche (severity: n/a; no done marker in file)
- TODO_list.txt:106 [ALGORITHME] Quiescence: autorisations standpat à revoir (severity: n/a; no done marker in file)
- TODO_list.txt:107 [ALGORITHME] Quiescence: depth < 0 à investiguer (severity: n/a; no done marker in file)
- TODO_list.txt:108 [ALGORITHME] Quiescence: tri coups (meilleures captures d'abord) (severity: n/a; no done marker in file)
- TODO_list.txt:109 [ALGORITHME] Ne plus regarder branches finies de GrogrosZero (severity: n/a; no done marker in file)
- TODO_list.txt:110 [ALGORITHME] Rechercher large au début, puis serré après (severity: n/a; no done marker in file)
- TODO_list.txt:111 [ALGORITHME] Retirer coups pourris pour augmenter capacité stockage (severity: n/a; no done marker in file)
- TODO_list.txt:112 [ALGORITHME] Prioriser coups non fully explorés (severity: n/a; no done marker in file)
- TODO_list.txt:113 [ALGORITHME] Transpositions: gérer overflows (severity: n/a; no done marker in file)
- TODO_list.txt:114 [ALGORITHME] Beta cut-off si en échec? (severity: n/a; no done marker in file)
- TODO_list.txt:115 [ALGORITHME] q_depth plus petit avec extensions (severity: n/a; no done marker in file)
- TODO_list.txt:116 [ALGORITHME] positions_history dans exploration plutôt que board (severity: n/a; no done marker in file)
- TODO_list.txt:117 [ALGORITHME] Pick_random_child en fonction de l'évaluation relative (severity: n/a; no done marker in file)
- TODO_list.txt:118 [ALGORITHME] Quand meilleur coup complètement exploré, refaire sans lui? (severity: n/a; no done marker in file)
- TODO_list.txt:119 [ALGORITHME] Utiliser améliorations/optimisations VisualStudio (severity: n/a; no done marker in file)
- TODO_list.txt:120 [ALGORITHME] GrogrosZero ralentit variantes longues -> optimiser (severity: n/a; no done marker in file)
- TODO_list.txt:121 [ALGORITHME] Recherche spécialement de mat (severity: n/a; no done marker in file)
- TODO_list.txt:122 [ALGORITHME] Raylib pour random (vérifier vitesse) (severity: n/a; no done marker in file)
- TODO_list.txt:123 [ALGORITHME] Initialiser plateau plutôt que from_fen (severity: n/a; no done marker in file)
- TODO_list.txt:124 [ALGORITHME] Undo gardent calculs GrogrosZero (severity: n/a; no done marker in file)
- TODO_list.txt:125 [ALGORITHME] Refaire get_moves (controlled squares) + perft test (severity: n/a; no done marker in file)
- TODO_list.txt:126 [ALGORITHME] Cleaning: flags coups, methods utiles, static constexpr (severity: n/a; no done marker in file)
- TODO_list.txt:127 [ALGORITHME] Refaire get_best_move pour correspondre aux vraies valeurs (WDL) (severity: n/a; no done marker in file)
- TODO_list.txt:128 [ALGORITHME] Réduire quiescence depth pour coups pourris (severity: n/a; no done marker in file)
- TODO_list.txt:129 [ALGORITHME] Gérer promotions de variantes (toute la branche + sélection bon noeud) (severity: n/a; no done marker in file)
- TODO_list.txt:130 [ALGORITHME] Implantation nouveaux passes de quiescence (severity: n/a; no done marker in file)
- TODO_list.txt:133 [INTERFACE] Readme pour les contrôles (severity: n/a; no done marker in file)
- TODO_list.txt:134 [INTERFACE] Sons: amélioration, maison, temps, ambiance, "soft" (severity: n/a; no done marker in file)
- TODO_list.txt:135 [INTERFACE] Images/nouvelles textures (severity: n/a; no done marker in file)
- TODO_list.txt:136 [INTERFACE] Options: désactivation son, menus, paramètres recherche IA (severity: n/a; no done marker in file)
- TODO_list.txt:137 [INTERFACE] Éditeur de positions (ajouter/supprimer pièces) (severity: n/a; no done marker in file)
- TODO_list.txt:138 [INTERFACE] Thread dédié pour l'affichage (PARALLÉLISER, lag énorme) (severity: n/a; no done marker in file)
- TODO_list.txt:139 [INTERFACE] Undo retire coup du PGN (severity: n/a; no done marker in file)
- TODO_list.txt:140 [INTERFACE] Textes couleurs différentes, plus de contraste (severity: n/a; no done marker in file)
- TODO_list.txt:141 [INTERFACE] Défiler variante au survol souris (severity: n/a; no done marker in file)
- TODO_list.txt:142 [INTERFACE] Montrer mate/pate sur échiquier (severity: n/a; no done marker in file)
- TODO_list.txt:143 [INTERFACE] Unload images/textures pour vider RAM (severity: n/a; no done marker in file)
- TODO_list.txt:144 [INTERFACE] Changer paramètres IA dans UI (severity: n/a; no done marker in file)
- TODO_list.txt:145 [INTERFACE] Sauvegarder parties entières dans fichier (severity: n/a; no done marker in file)
- TODO_list.txt:146 [INTERFACE] Analyses MC: montrer chemin meilleure éval et chemin joué (severity: n/a; no done marker in file)
- TODO_list.txt:147 [INTERFACE] Grab slider / cliquer pour changer place (severity: n/a; no done marker in file)
- TODO_list.txt:148 [INTERFACE] Batailles entre paramètres d'évaluation (severity: n/a; no done marker in file)
- TODO_list.txt:149 [INTERFACE] Import/export PGN (severity: n/a; no done marker in file)
- TODO_list.txt:150 [INTERFACE] Afficher réflexion Grogros sur PGN (severity: n/a; no done marker in file)
- TODO_list.txt:151 [INTERFACE] Changer nombre de noeuds / profondeur dans GUI (severity: n/a; no done marker in file)
- TODO_list.txt:152 [INTERFACE] Smooth barre d'évaluation (severity: n/a; no done marker in file)
- TODO_list.txt:153 [INTERFACE] Graphe d'éval en fin de partie (severity: n/a; no done marker in file)
- TODO_list.txt:154 [INTERFACE] Échelle logarithmique barre d'éval (severity: n/a; no done marker in file)
- TODO_list.txt:155 [INTERFACE] Fixer gestion du temps (affiché != vrai) (severity: n/a; no done marker in file)
- TODO_list.txt:156 [INTERFACE] Clean implémentation GUI (severity: n/a; no done marker in file)
- TODO_list.txt:157 [INTERFACE] Pre-moves depuis zéro (+ multiples) (severity: n/a; no done marker in file)
- TODO_list.txt:158 [INTERFACE] Flèches: variante calculée, coups principaux (severity: n/a; no done marker in file)
- TODO_list.txt:159 [INTERFACE] Montrer la menace (changer le trait) (severity: n/a; no done marker in file)
- TODO_list.txt:160 [INTERFACE] Animations Grogros qui bouge les pièces (severity: n/a; no done marker in file)
- TODO_list.txt:161 [INTERFACE] Désélectionner pièce après coup joué (severity: n/a; no done marker in file)
- TODO_list.txt:162 [INTERFACE] Int overflow dans softmax (severity: n/a; no done marker in file)
- TODO_list.txt:163 [INTERFACE] Affichage en fonction de l'arbre de jeu (severity: n/a; no done marker in file)
- TODO_list.txt:164 [INTERFACE] Time management en fonction différence meilleur/autres coups (severity: n/a; no done marker in file)
- TODO_list.txt:165 [INTERFACE] Naviguer dans variante: cliquer, changer, dé-surligner (severity: n/a; no done marker in file)
- TODO_list.txt:166 [INTERFACE] From_fen -> reset l'arbre (severity: n/a; no done marker in file)
- TODO_list.txt:167 [INTERFACE] Garder réflexions GrogrosZero en navigation (severity: n/a; no done marker in file)
- TODO_list.txt:168 [INTERFACE] Passer variante en principale (severity: n/a; no done marker in file)
- TODO_list.txt:169 [INTERFACE] Montrer ligne montante/descendante (severity: n/a; no done marker in file)
- TODO_list.txt:170 [INTERFACE] Sons quand on navigue (severity: n/a; no done marker in file)
- TODO_list.txt:171 [INTERFACE] Afficher forces/faiblesses (vert/orange/rouge) (severity: n/a; no done marker in file)
- TODO_list.txt:172 [INTERFACE] Afficher coup courant dans variante (severity: n/a; no done marker in file)
- TODO_list.txt:173 [INTERFACE] Variante avec détails (profondeur) (severity: n/a; no done marker in file)
- TODO_list.txt:174 [INTERFACE] Temps + noeuds + éval dans PGN (severity: n/a; no done marker in file)
- TODO_list.txt:175 [INTERFACE] Pièces UTF-8 visuel amélioré (severity: n/a; no done marker in file)
- TODO_list.txt:176 [INTERFACE] Colorer cases selon bonus pièces (severity: n/a; no done marker in file)
- TODO_list.txt:177 [INTERFACE] FEN auto-complétion si incorrect (severity: n/a; no done marker in file)
- TODO_list.txt:178 [INTERFACE] Afficher info emergency cutoff (severity: n/a; no done marker in file)
- TODO_list.txt:179 [INTERFACE] Mode analyse POV noirs (bug) (severity: n/a; no done marker in file)
- TODO_list.txt:180 [INTERFACE] Afficher site de binding (severity: n/a; no done marker in file)
- TODO_list.txt:181 [INTERFACE] Annuler bindings (CTRL+Y) (severity: n/a; no done marker in file)
- TODO_list.txt:182 [INTERFACE] Numéro de version dans UI (severity: n/a; no done marker in file)
- TODO_list.txt:183 [INTERFACE] Défiler variante avec flèche maintenue (severity: n/a; no done marker in file)
- TODO_list.txt:184 [INTERFACE] Textures GPU + shaders (outline, texte non flou) (severity: n/a; no done marker in file)
- TODO_list.txt:185 [INTERFACE] Bug UI 12 FPS -> fix (severity: n/a; no done marker in file)
- TODO_list.txt:186 [INTERFACE] Lignes bézier/cercles non beaux -> améliorer (severity: n/a; no done marker in file)
- TODO_list.txt:189 [INFRASTRUCTURE] Séparer board.cpp en fichiers dédiés (GUI, IA, evaluation) (severity: n/a; no done marker in file)
- TODO_list.txt:190 [INFRASTRUCTURE] Faire une classe pour les IA (severity: n/a; no done marker in file)
- TODO_list.txt:191 [INFRASTRUCTURE] Quel compilateur le plus rapide/opti (severity: n/a; no done marker in file)
- TODO_list.txt:192 [INFRASTRUCTURE] Passer commentaires en anglais + clean code (severity: n/a; no done marker in file)
- TODO_list.txt:193 [INFRASTRUCTURE] Noms de version sur Grogros (severity: n/a; no done marker in file)
- TODO_list.txt:194 [INFRASTRUCTURE] Unit tests pour les fonctions (severity: n/a; no done marker in file)
- TODO_list.txt:195 [INFRASTRUCTURE] Tests micro de l'algorithme (efficience à temps minimal) (severity: n/a; no done marker in file)
- TODO_list.txt:196 [INFRASTRUCTURE] Documentation complète + refactorisation (severity: n/a; no done marker in file)
- TODO_list.txt:197 [INFRASTRUCTURE] Parallélisation propre (severity: n/a; no done marker in file)
- TODO_list.txt:198 [INFRASTRUCTURE] Profiler pour fonctions lentes (severity: n/a; no done marker in file)
- TODO_list.txt:199 [INFRASTRUCTURE] Nettoyer code exploration (severity: n/a; no done marker in file)
- TODO_list.txt:200 [INFRASTRUCTURE] Clean global (commentaires inutiles, factoriser, opti micro) (severity: n/a; no done marker in file)
- TODO_list.txt:201 [INFRASTRUCTURE] Optimiser évaluation (severity: n/a; no done marker in file)
- TODO_list.txt:202 [INFRASTRUCTURE] Framework de test (severity: n/a; no done marker in file)
- TODO_list.txt:203 [INFRASTRUCTURE] Test vitesse évaluation (severity: n/a; no done marker in file)
- TODO_list.txt:204 [INFRASTRUCTURE] Fonction test: comparer évals Grogros vs Leela (severity: n/a; no done marker in file)
- TODO_list.txt:205 [INFRASTRUCTURE] Download positions pour tests (severity: n/a; no done marker in file)
- TODO_list.txt:206 [INFRASTRUCTURE] Tracker fuites mémoire (severity: n/a; no done marker in file)
- TODO_list.txt:207 [INFRASTRUCTURE] robin_maps au lieu de maps (severity: n/a; no done marker in file)
- TODO_list.txt:208 [INFRASTRUCTURE] Screenshots dans mauvais dossier -> fix (severity: n/a; no done marker in file)
- TODO_list.txt:209 [INFRASTRUCTURE] Crash quand on ferme la fenêtre (severity: n/a; no done marker in file)
- TODO_list.txt:212 [RESEAUX NEURONES] Matches avec 0 réflexion, seulement le réseau (severity: n/a; no done marker in file)
- TODO_list.txt:213 [RESEAUX NEURONES] Symétrie dans le réseau? (severity: n/a; no done marker in file)
- TODO_list.txt:214 [RESEAUX NEURONES] Apprentissage: comment le faire? (severity: n/a; no done marker in file)
- TODO_list.txt:215 [RESEAUX NEURONES] Parties contre lui-même (severity: n/a; no done marker in file)
- TODO_list.txt:216 [RESEAUX NEURONES] Algorithme génétique (détruire faibles, mutations forts) (severity: n/a; no done marker in file)
- TODO_list.txt:217 [RESEAUX NEURONES] Bonus/malus quand il a mal évalué (severity: n/a; no done marker in file)
- TODO_list.txt:218 [RESEAUX NEURONES] Apprentissage fin de partie puis remonter (severity: n/a; no done marker in file)
- TODO_list.txt:219 [RESEAUX NEURONES] Reinforcement learning (severity: n/a; no done marker in file)
- TODO_list.txt:220 [RESEAUX NEURONES] Paramètres au-delà des pièces (trait, roques) (severity: n/a; no done marker in file)
- TODO_list.txt:221 [RESEAUX NEURONES] Peu de valeurs négatives dans les évals: normal? (severity: n/a; no done marker in file)
- TODO_list.txt:224 [BUGS] #1 Temps: parfois s'écoule quasi plus (itérations p.s. vs noeuds p.s.) (severity: n/a; no done marker in file)
- TODO_list.txt:225 [BUGS] #2 Profondeur des variantes à vérifier (severity: n/a; no done marker in file)
- TODO_list.txt:226 [BUGS] #3 Après répétition, Grogros ne joue plus (severity: n/a; no done marker in file)
- TODO_list.txt:227 [BUGS] #4 Promotions de variantes à revoir (severity: n/a; no done marker in file)
- TODO_list.txt:228 [BUGS] #5 Même coup 2× variantes différentes = même parent → retour arrière mauvais endroit (severity: n/a; no done marker in file)
- TODO_list.txt:229 [BUGS] #6 Plusieurs eval mats → joue lentement (evals proches) (severity: n/a; no done marker in file)
- TODO_list.txt:230 [BUGS] #7 Parfois game reset (severity: n/a; no done marker in file)
- TODO_list.txt:231 [BUGS] #8 c3: faut prendre h8 (severity: n/a; no done marker in file)
- TODO_list.txt:232 [BUGS] #9 Eval = ±INT_MAX alors que pas normal (severity: n/a; no done marker in file)
- TODO_list.txt:233 [BUGS] #10 Slider PGN buggé (severity: n/a; no done marker in file)
- TODO_list.txt:234 [BUGS] #11 Buffer parfois full quand même (severity: n/a; no done marker in file)
- TODO_list.txt:235 [BUGS] #12 Transpositions: croit win alors que nulle + clic renvoie position corrompue (6k1/8/7P/7K/8/8/8/8 w - - 3 72) (severity: n/a; no done marker in file)
- TODO_list.txt:236 [BUGS] #13 Transpo n'augmente pas profondeur en endgame (severity: n/a; no done marker in file)
- TODO_list.txt:237 [BUGS] #14 8/6pk/7p/8/1bQB3P/p1p3P1/5P1K/1q6 w - -: croit Df7 gagne (severity: n/a; no done marker in file)
- TODO_list.txt:238 [BUGS] #15 Retour arrière crash (severity: n/a; no done marker in file)
- TODO_list.txt:239 [BUGS] #16 Transpo pleine → comportement bizarre (8/2Q3pk/7p/8/1b1B3P/p1p3P1/5P1K/1q6 b - -: Ff8→0%) (severity: n/a; no done marker in file)
- TODO_list.txt:240 [BUGS] #17 bxa5 pas regardé (r3k1nr/ppp2ppp/1b1p1q2/n3p3/1PB1P1b1/2P2N2/3P1PPP/RNBQ1RK1 w kq - 0 9) (severity: n/a; no done marker in file)
- TODO_list.txt:241 [BUGS] #18 Transposition met -M4 temporairement sur coup sans mat (2Q5/4b1pk/7p/7P/3B4/p1p3P1/5P1K/1q6 b - -) (severity: n/a; no done marker in file)
- TODO_list.txt:242 [BUGS] #19 Fa6 -M4 puis +M4 confidence 87% (r1b3k1/p4ppp/5n2/1Rp4q/4B3/2P5/P1PQ2PP/4R1K1 b - - 3 21) (severity: n/a; no done marker in file)
- TODO_list.txt:243 [BUGS] #20 Transpositions: test (8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7) (severity: n/a; no done marker in file)
- TODO_list.txt:244 [BUGS] #21 Couleur oscille bleu/magenta sur répétition (severity: n/a; no done marker in file)
- TODO_list.txt:245 [BUGS] #22 Parfois crash (severity: n/a; no done marker in file)
- TODO_list.txt:246 [BUGS] #23 Temps s'arrête après répétition (pas toujours) (severity: n/a; no done marker in file)
- TODO_list.txt:247 [BUGS] #24 Pièce sans mouvement: grave si pour toujours (severity: n/a; no done marker in file)
- TODO_list.txt:248 [BUGS] #25 Crash encore (severity: n/a; no done marker in file)
- TODO_list.txt:249 [BUGS] #26 Positions évaluées plusieurs fois (severity: n/a; no done marker in file)
- TODO_list.txt:250 [BUGS] #30 Quiescence: Fd3 puis Fxc3 = mauvaise quiescence (severity: n/a; no done marker in file)
- TODO_list.txt:251 [BUGS] #31 Profondeur 1 partout parfois (severity: n/a; no done marker in file)
- TODO_list.txt:252 [BUGS] #32 Bug from_fen après réflexion (severity: n/a; no done marker in file)
- TODO_list.txt:253 [BUGS] #33 Bug planter UI (severity: n/a; no done marker in file)
- TODO_list.txt:254 [BUGS] #34 is_stand_pat_eval commenté dans quiescence (severity: n/a; no done marker in file)
- TODO_list.txt:255 [BUGS] #35 TODO/REVIEW/FIXME en plein milieu du code (severity: n/a; no done marker in file)
- TODO_list.txt:256 [BUGS] #36 L'ordi plante quand on passe sur autre fenêtre (severity: n/a; no done marker in file)
- TODO_list.txt:257 [BUGS] #37 Historique encore tout cassé (severity: n/a; no done marker in file)
- TODO_list.txt:258 [BUGS] #38 Temps de réflexion incorrect (severity: n/a; no done marker in file)
- TODO_list.txt:259 [BUGS] #39 Variante transposée: un seul parent mis à jour (severity: n/a; no done marker in file)
- TODO_list.txt:260 [BUGS] #40 Alpha-beta pruning quiescence search à fix (severity: n/a; no done marker in file)
- TODO_list.txt:261 [BUGS] #41 Quiescence regarde échecs plein de fois en fin roi+tour (hyper lent) (severity: n/a; no done marker in file)
- TODO_list.txt:262 [BUGS] #42 Après jouer coups + réflexion depuis autre point ≠ résultat (severity: n/a; no done marker in file)
- TODO_list.txt:263 [BUGS] #43 Repet: _can_explore false pas vérifié (pareil mats/branches pleines) (severity: n/a; no done marker in file)
- TODO_list.txt:264 [BUGS] #44 12 FPS UI quand grogros inactif + sélection pièces: légaux pas affichés (severity: n/a; no done marker in file)

### B4) ROADMAP.md (A1-A4, items 1-3 and 10 excluded as DONE)
- ROADMAP A5 - 10,2,2 and 10,6,2 vs 10,2,0 re-gate: decision recorded (default 10,6,2), no done marker - no file:line - ROADMAP.md:37 - severity n/a
- ROADMAP #229 - hard 6-ply quiet sequence (Qb3 missed), DEPRIORITIZED - no file:line - ROADMAP.md:213-218 - severity n/a
- ROADMAP convergence - "vrais rates": needs about 4x iterations per 0.1 s (NPS, parallel, or NN ordering) - no file:line - ROADMAP.md:232-245 - severity n/a
- ROADMAP plan step 1 - quiescence node waste (8.8x) to trim; king-safety cache removal "a decider" - ROADMAP.md:247-258 - severity n/a
- ROADMAP plan steps 2-4 - hotpath profiling, parallelisation, NN ordering (open) - ROADMAP.md:266-268 - severity n/a
- ROADMAP Objectif 1M NPS - target, open - ROADMAP.md:270-273 - severity n/a
- ROADMAP Phase 6 shared tree (Lazy SMP) - conditional next step "si go" - ROADMAP.md:285-288 - severity n/a
- ROADMAP 1.3 Move ordering improvements (TT move, killers, history, countermove) - ROADMAP.md:306 - severity n/a
- ROADMAP 2.1 Selective deepening refinement - ROADMAP.md:326 - severity n/a
- ROADMAP 2.2 Quiescence optimisation - ROADMAP.md:331 - severity n/a
- ROADMAP 2.3 Main search pruning (NMP, LMR, futility, razoring) - ROADMAP.md:337 - severity n/a
- ROADMAP Phase 3 TODO - 5 unchecked boxes (TT main search test, DAG test, node savings, bugs, TT works) - ROADMAP.md:349-354 - severity n/a
- ROADMAP Phase 4 - neural network evaluation (LAST) - ROADMAP.md:362 - severity n/a
- ROADMAP Phase 5 - UI tunability ("next") - ROADMAP.md:397 - severity n/a
- ROADMAP Phase 6 - parallelisation - ROADMAP.md:403 - severity n/a
- ROADMAP Phase 7 - NPS optimisation - ROADMAP.md:435 - severity n/a
- ROADMAP priority 1 (marked DONE) - remaining: reproduce random-position corruption, identify root cause - ROADMAP.md:483 - severity n/a
- ROADMAP priority 4 - selective deepening re-evaluation - ROADMAP.md:496 - severity n/a
- ROADMAP priority 5 - full algorithmic audit, focus on correctness bugs - ROADMAP.md:502 - severity n/a
- ROADMAP priority 6 - node wastage optimisation - ROADMAP.md:507 - severity n/a
- ROADMAP priority 7 - TT + DAG audit (check TT storage/lookup mistakes) - ROADMAP.md:512 - severity n/a
- ROADMAP priority 8 - CPU hotpath and RAM - ROADMAP.md:517 - severity n/a
- ROADMAP priority 9 - parallelisation - ROADMAP.md:522 - severity n/a
- ROADMAP success criteria - 9 unchecked boxes - ROADMAP.md:544-552 - severity n/a
