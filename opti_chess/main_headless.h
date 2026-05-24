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
#include "gui.h" // main_GUI / GameTree : from_fen() déréférence main_GUI._game_tree
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

    // Trajectoire par batch : on imprime au plus ~20 points + le premier et le
    // dernier, pour VOIR une dérive (ex. un gain qui s'érode vers la nulle au fil
    // des itérations) plutôt qu'un seul chiffre final. `_nodes` = proxy d'effort.
    const int print_stride = (n_batches <= 25) ? 1 : (n_batches / 20);
    int eval_value = 0;
    for (int b = 0; b < n_batches; ++b) {
        root->grogros_zero(&monte_board_buffer, eval, ALPHA, BETA, GAMMA,
                           iters_per_batch, QDEPTH);
        eval_value = root->_deep_evaluation._value;
        if (b == 0 || b == n_batches - 1 || (b % print_stride) == 0) {
            std::printf("[DAG-TEST]   %-22s dag=%d  batch=%3d  eval=%6d  avg=%.3f  nodes=%d\n",
                        name, dag_on ? 1 : 0, b, eval_value,
                        (double)root->_deep_evaluation._avg_score, root->_nodes);
        }
    }
    std::printf("[DAG-TEST] %-26s dag=%d  FINAL eval=%6d  avg=%.3f  nodes=%d\n",
                name, dag_on ? 1 : 0, eval_value,
                (double)root->_deep_evaluation._avg_score, root->_nodes);
    return eval_value;
}

} // namespace dag_headless

inline int main_headless(int argc, char** argv) {
    using namespace dag_headless;

    // Override optionnel des budgets pour itérer sans recompiler :
    // `--dag-test [n_batches] [iters_per_batch]` (appliqué aux DEUX repros).
    // Sans argument : budgets par défaut (miroir de GUI::run_dag_repro_1/2).
    const int ov_batches = (argc > 2) ? std::atoi(argv[2]) : 0;
    const int ov_iters   = (argc > 3) ? std::atoi(argv[3]) : 0;

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
    const int b1 = ov_batches ? ov_batches : 10;
    const int i1 = ov_iters   ? ov_iters   : 2000;
    const int b2 = ov_batches ? ov_batches : 20;
    const int i2 = ov_iters   ? ov_iters   : 3000;
    // Repetition draws hors finale-de-pions (cas general GHI : noeuds PARTAGES sous DAG).
    // Verifies OFF=0 ON=0 (calibration 2026-05-24, Tests.txt:62 et :1423).
    // fen3 = "tour folle" : nulle par perpetuelle alors que les Noirs ont une dame de plus
    //        -> la nulle par repetition doit primer un enorme desavantage materiel.
    const std::string fen3 = "8/8/8/7k/8/qp6/p7/K5R1 w - - 0 1";
    // fen4 = perpetuelle plateau plein (tours doublees en 7e) -> riche en transpositions.
    const std::string fen4 = "rn2k1r1/pppRpRpp/bb6/q7/6n1/8/PPPPPPPP/1NB1KBN1 w q - 0 1";

    const int r1_off = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, b1, i1, false);
    const int r1_on  = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, b1, i1, true);
    const int r2_off = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, b2, i2, false);
    const int r2_on  = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, b2, i2, true);
    // node_map size right after the ON pawn-endgame run = sharing-active proxy.
    const size_t r2_on_nodemap = node_map.size();
    const int r3_off = run_repro(root, board, &eval, "repro3_perpetual", fen3, b2, i2, false);
    const int r3_on  = run_repro(root, board, &eval, "repro3_perpetual", fen3, b2, i2, true);
    const int r4_off = run_repro(root, board, &eval, "repro4_perpetual", fen4, b2, i2, false);
    const int r4_on  = run_repro(root, board, &eval, "repro4_perpetual", fen4, b2, i2, true);

    int fails = 0;
    auto check = [&](bool ok, const char* desc) {
        std::printf("[DAG-TEST] %-44s %s\n", desc, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    };

    check(std::abs(r1_off) <= EPS_DRAW,                              "Repro1 OFF ~ draw (sanity)");
    check(std::abs(r1_on)  <= EPS_DRAW,                              "Repro1 ON  ~ draw (THE FIX)");
    check(r2_off >= WIN_THRESH,                                      "Repro2 OFF winning (sanity)");
    check(r2_on  >= WIN_THRESH && std::abs(r2_on) > EPS_DRAW,        "Repro2 ON  winning (no phantom draw)");
    check(std::abs(r3_off) <= EPS_DRAW,                              "Repro3 OFF perpetual ~ draw (sanity)");
    check(std::abs(r3_on) <= EPS_DRAW,                               "Repro3 ON  perpetual ~ draw");
    check(std::abs(r4_off) <= EPS_DRAW,                              "Repro4 OFF perpetual ~ draw (sanity)");
    check(std::abs(r4_on) <= EPS_DRAW,                               "Repro4 ON  perpetual ~ draw");
    std::printf("[DAG-TEST] sharing-active: node_map=%zu after repro2 ON\n", r2_on_nodemap);
    check(r2_on_nodemap > 100,                                       "Sharing active under DAG (depth not dead)");

    std::printf("[DAG-TEST] %d failure(s)\n", fails);
    return fails == 0 ? 0 : 1;
}
