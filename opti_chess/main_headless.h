#pragma once

// #11 attempt-8 — banc de validation headless (design 2026-05-22 §5).
// Pas de fenêtre raylib : réutilise les buffers globaux (monte_board_buffer,
// monte_node_buffer, node_map) + grogros_zero, sans rien dessiner. Lancé via
// `opti_chess --dag-test`. Sortie != 0 si une assertion échoue -> exécutable
// en CLI/CI par l'assistant, ce qui casse le goulot du gate GUI manuel.

#include "board.h"
#include "exploration.h"
#include "evaluation.h"
#include "buffer.h"
#include "zobrist.h"
#include <cmath>
#include <cstdio>
#include <string>

namespace dag_headless {

// Seuils épinglés depuis les trajectoires OFF/ON mesurées (design §5.3 ;
// faux-gain Repro 1 ON mesuré = 476 ; Repro 2 ON atteint ~298). Confirmés/
// ajustés au Step 4 d'après la sortie réelle.
constexpr int EPS_DRAW   = 50;  // |eval| <= EPS_DRAW  => nulle
                                // (calibré sur repro1_OFF=0 et repro2_ON=101;
                                //  doit rester < 101 pour ne pas confondre 101
                                //  avec une nulle, et nettement < 476 le faux-gain)
constexpr int WIN_THRESH = 70;  // eval  >= WIN_THRESH => gain net
                                // (calibré sous repro2 min=101, au-dessus de EPS_DRAW)

// Paramètres de recherche : miroir exact de gui.h:98-105.
constexpr double ALPHA  = 0.00001;
constexpr double BETA   = 5.0;
constexpr double GAMMA  = 1.10;
constexpr int    QDEPTH = 10;

// Charge la FEN sur (root, board) déjà alloués, force le toggle DAG, tourne
// n_batches × iters_per_batch itérations, renvoie l'éval racine finale.
// Reset d'état entre cas = miroir de GUI::load_FEN (gui.cpp:1650) + du handler
// DELETE de la GUI (main_gui.h:437-438) : node_map + TT purgés, root reset.
inline int run_repro(Node* root, Board* board, Evaluator* eval,
                     const char* name, const std::string& fen,
                     int n_batches, int iters_per_batch, bool dag_on) {
    root->reset();
    root->_board = board;
    board->from_fen(fen);
    root->_is_active = true;
    board->_is_active = true;
    node_map.clear();
    transposition_table.clear();

    g_tt_node_dag = dag_on;
    g_tt_main_search = false;

    for (int b = 0; b < n_batches; ++b) {
        root->grogros_zero(&monte_board_buffer, eval, ALPHA, BETA, GAMMA,
                           iters_per_batch, QDEPTH);
    }

    const int eval_value = root->_deep_evaluation._value;
    std::printf("[DAG-TEST] %-26s dag=%d  eval=%6d  avg=%.3f\n",
                name, dag_on ? 1 : 0, eval_value,
                root->_deep_evaluation._avg_score);
    return eval_value;
}

} // namespace dag_headless

inline int main_headless() {
    using namespace dag_headless;

    // Init des pools globaux : miroir de GUI::init_buffers (gui.cpp:2051).
    const PoolSizing ps = compute_pool_sizing();
    if (!monte_board_buffer._init) monte_board_buffer.init(ps.board_length);
    if (!monte_node_buffer._init)  monte_node_buffer.init(ps.node_length);
    transposition_table.init(ps.tt_length, nullptr, true);

    Evaluator eval; // défaut, comme gui.h:167 (new Evaluator()).

    Board* board = monte_board_buffer.get_first_free_board();
    Node*  root  = monte_node_buffer.get_first_free_node();

    // Board::from_fen() appelle main_GUI._game_tree.new_tree(*this), ce qui
    // déréférence main_GUI._game_tree._root (non initialisé par le ctor par
    // défaut de GameTree). On l'initialise ici comme le fait main_ui() ligne 94.
    main_GUI._game_tree = GameTree(*board);

    // Repro 1 : KP(h)-vs-K, nulle théorique. Repro 2 : finale de pions gagnée.
    const std::string fen1 = "6k1/8/7P/7K/8/8/8/8 w - - 3 72";
    const std::string fen2 = "8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7";

    // Mêmes budgets que GUI::run_dag_repro_1/2 (gui.cpp:1166-1187).
    const int r1_off = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, 10, 2000, false);
    const int r1_on  = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, 10, 2000, true);
    const int r2_off = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, 20, 3000, false);
    const int r2_on  = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, 20, 3000, true);

    int fails = 0;
    auto check = [&](bool ok, const char* desc) {
        std::printf("[DAG-TEST] %-44s %s\n", desc, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    };

    check(std::abs(r1_off) <= EPS_DRAW,                              "Repro1 OFF ~ draw (sanity)");
    check(std::abs(r1_on)  <= EPS_DRAW,                              "Repro1 ON  ~ draw (THE FIX)");
    check(r2_off >= WIN_THRESH,                                      "Repro2 OFF winning (sanity)");
    check(r2_on  >= WIN_THRESH && std::abs(r2_on) > EPS_DRAW,        "Repro2 ON  winning (no phantom draw)");

    std::printf("[DAG-TEST] %d failure(s)\n", fails);
    return fails == 0 ? 0 : 1;
}
