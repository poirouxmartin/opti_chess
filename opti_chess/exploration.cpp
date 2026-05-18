#include "exploration.h"
#include "useful_functions.h"
#include "zobrist.h"

namespace {

constexpr uint8_t search_repetition_limit = 2;

// #11 Plan B — seuil de coupe AFFICHAGE (get_exploration_variants /
// get_main_depth) decouple de l'elagage de recherche. search_repetition_limit
// (double, agressif) tronquerait la PV des la 2e occurrence -> en finale les
// manoeuvres de roi rebouclent en ~2-3 coups et la variante principale est
// coupee tres tot meme si la vraie ligne est longue. On affiche jusqu'a la
// VRAIE nulle (triple = regle FIDE) ; borne toujours les vrais cycles bien
// avant le cap max_depth=500. Applique seulement sous DAG (cf. usage).
constexpr uint8_t display_repetition_limit = 3;

// #3 : un score de mat encode sa distance via _moves_count (board.cpp:1569),
// or _moves_count est ABSENT de la clé Zobrist. Sans normalisation, une même
// position atteinte par un chemin de longueur différente relit depuis la TT
// une distance de mat fausse ("fantômes"). On canonise au store (retrait de
// l'ancrage _moves_count -> ne dépend plus que de D = distance à mat, qui est
// une propriété de la position) ; au probe on reconstruit avec le _moves_count
// du nœud courant. Seuil mat = idiome is_eval_mate / #1 (10*|e| > mate_value),
// robuste : |stored| ~ mate_value(1e8) - D·mate_ply + mc·mate_ply, mc/D petits.
inline int tt_normalize_mate(int eval, int moves_count) {
	if (10 * abs(eval) > mate_value)
		return eval + (eval > 0 ? 1 : -1) * moves_count * mate_ply;
	return eval;
}
inline int tt_denormalize_mate(int eval, int moves_count) {
	if (10 * abs(eval) > mate_value)
		return eval - (eval > 0 ? 1 : -1) * moves_count * mate_ply;
	return eval;
}

// #11 Plan A — TT scalaire dans la recherche principale.
// QDEPTH_BAND : décale toute profondeur de write-back MCTS au-dessus de la
// bande de quiescence (depth quiescence <= _quiescence_depth, ~10). Le
// remplacement depth-preferred (zobrist.cpp:113) garde ainsi toujours une
// entrée raffinée plutôt qu'une feuille de quiescence, et la porte de
// réutilisation peut exiger un vrai sous-arbre raffiné.
constexpr int QDEPTH_BAND = 256;
// MIN_REUSE_LOG2 : on ne réutilise que les entrées représentant >= 2^N noeuds
// raffinés (jamais une simple feuille de quiescence). Ajustable au gate.
constexpr int MIN_REUSE_LOG2 = 4;

// log2 entier de (nodes+1), borné. Pas de float (hot-path-friendly, MSVC).
inline int tt_writeback_depth(int nodes) {
	int v = (nodes > 0 ? nodes : 0) + 1; // nodes négatifs = bug amont : on dégrade proprement (depth = QDEPTH_BAND)
	int log2v = 0;
	while (v > 1) { v >>= 1; ++log2v; }
	return QDEPTH_BAND + log2v;
}

// Cohérence des champs dérivés d'une Evaluation dont _value vient de la TT.
// Factorisation EXACTE du bloc de synthèse du cutoff quiescence (#1 v2 / #14) :
// _value est supposé déjà posé (white-relative). On force _uncertainty=0 (une
// valeur TT fiable ne doit pas être filtrée par l'incertitude statique), on
// remet _winnable_* par signe si mat (évite le scaling sur un score de mat
// géant ; hors mat _winnable_* sont conservés, propriété de position), puis
// on redérive _wdl / _avg_score depuis _value.
inline void tt_fixup_derived(Evaluation& e) {
	e._uncertainty = 0.0f;
	if (10 * abs(e._value) > mate_value) {
		e._winnable_white = e._value > 0 ? 1.0f : 0.0f;
		e._winnable_black = e._value < 0 ? 1.0f : 0.0f;
	}
	e.get_WDL();
	e.get_average_score();
}

uint8_t position_history_count(const PositionHistory& path_history, Board& board) {
	board.get_zobrist_key();
	const auto it = path_history.find(board._zobrist_key);
	return it == path_history.end() ? 0 : it->second;
}

} // namespace

// The repetition state is path-local: it depends on the exact move sequence that led to
// the current node, so it must stay outside Node/Board state if we want transpositions later.
void ensure_position_in_history(PositionHistory& path_history, Board& board) {
	board.get_zobrist_key();
	path_history.try_emplace(board._zobrist_key, 1);
}

void record_position_in_history(PositionHistory& path_history, Board& board) {
	board.get_zobrist_key();
	path_history[board._zobrist_key]++;
}

// #7 / Plan B-1 — annule un push (record) du path history (pop). Clé passée
// directement (pas de Board) : symétrique exact de record, simple lookup,
// efface l'entrée à 0 pour borner la taille de la map.
void unrecord_position_in_history(PositionHistory& path_history, uint64_t key) {
	const auto it = path_history.find(key);
	if (it == path_history.end()) {
		return; // appel non équilibré : ne devrait jamais arriver
	}
	if (it->second <= 1) {
		path_history.erase(it);
	}
	else {
		it.value()--;
	}
}

// RAII : push (record) la position du board à la construction, pop à la
// destruction. La clé Zobrist est capturée À LA CONSTRUCTION (get_zobrist_key
// n'est pas idempotent : recalcul O(64)) — le dtor ne retouche pas le Board,
// l'appariement push/pop est structurel. Équilibrage garanti sur TOUT chemin
// de sortie (returns anticipés inclus) — voir « Balance invariant » du plan.
struct PathScope {
	PositionHistory& _history;
	uint64_t _key;
	PathScope(PositionHistory& history, Board& board) : _history(history) {
		board.get_zobrist_key();
		_key = board._zobrist_key;
		_history[_key]++;
	}
	~PathScope() {
		unrecord_position_in_history(_history, _key);
	}
	PathScope(const PathScope&) = delete;
	PathScope& operator=(const PathScope&) = delete;
};

bool position_is_draw_by_repetition(const PositionHistory& path_history, Board& board, uint8_t repetition_limit = search_repetition_limit) {
	return position_history_count(path_history, board) + 1 >= repetition_limit;
}

void mark_position_as_draw(Board& board) {
	board._game_over_value = draw;
	board._game_over_checked = true;
}

void init_terminal_draw_child(Node* child, Board* board, Evaluator* eval, Network* network) {
	mark_position_as_draw(*board);

	child->_board = board;
	child->evaluate_position(eval, false, network, true);
	child->_fully_explored = true;
	child->_can_explore = false;
	child->_is_terminal = true;
}

// #11 Plan A — feuille gelée portant une valeur fiable issue de la TT.
// Structurellement calquée sur init_terminal_draw_child. Détection terminale
// d'abord : Board::evaluate(check_game_over=true) ne CALCULE pas la fin de
// partie (is_game_over() y est commenté, board.cpp:1548) ; il ne fait que LIRE
// _game_over_value. On le calcule donc explicitement comme init_node
// (get_moves puis is_game_over) avant d'évaluer. Si la position est réellement
// terminale, Board::evaluate a déjà posé l'éval exacte (mat/nulle), meilleure
// que le scalaire TT : on la garde et on pose _is_terminal=true. Sinon on
// remplace _deep_evaluation par la valeur TT (white-relative) avec les champs
// dérivés cohérents (#1/#14). Compteurs identiques à la feuille de nulle
// (_nodes=1, _iterations=1) : le terme d'exploration UCT (voir
// pick_random_child, terme d'exploration sur _iterations) se comporte alors
// comme pour les feuilles terminales/nulles existantes.
void init_tt_leaf_child(Node* child, Board* board, Evaluator* eval, Network* network, int white_relative_value) {
	child->_board = board;

	// Fin de partie : Board::evaluate ne la calcule pas (cf. ci-dessus), on
	// reproduit la séquence de init_node avant d'évaluer.
	board->get_moves();
	board->is_game_over();

	child->evaluate_position(eval, false, network, true);

	if (board->_game_over_value != unterminated) {
		// Réellement terminale : éval exacte (mat/nulle) déjà posée par
		// Board::evaluate, supérieure au scalaire TT — on ne l'écrase pas.
		child->_is_terminal = true;
	}
	else {
		// _deep_evaluation == _static_evaluation ici (evaluate_position le copie
		// quand static_only=false) ; copie explicite conservée à dessein : si
		// ce défaut changeait, c'est elle qui garantit une base cohérente avant
		// de substituer la valeur TT et d'appliquer tt_fixup_derived (#1/#14).
		child->_deep_evaluation = child->_static_evaluation;
		child->_deep_evaluation._value = white_relative_value;
		child->_deep_evaluation._evaluated = true;
		tt_fixup_derived(child->_deep_evaluation);
	}

	child->_nodes = 1;
	child->_iterations = 1;
	child->_is_stand_pat_eval = false;
	child->_fully_explored = true;
	child->_can_explore = false;
	// Gel durable : sans _initialized=true, grogros_zero relancerait
	// quiescence (écrasant la valeur TT) si la feuille est revisitée comme
	// best_move. Couplé au retour anticipé !_can_explore de grogros_zero
	// la feuille reste réellement gelée.
	child->_initialized = true;
}

// Constructeur par défaut
Node::Node() {
}

// Constructeur avec un plateau
Node::Node(Board *board) {
	_board = board;
}

// Fonction qui ajoute un fils
void Node::add_child(Node* child, Move move) {
	// FIXME: vérifier si le coup n'est pas déjà dans les enfants?
	if (_children.contains(move)) {
		cout << "move already in children" << endl;
        return;
	}

	if (child == nullptr) {
		cout << "WHY DO WE ADD A NULL CHILD??" << endl;
		return;
	}

	if (move.is_null_move()) {
		cout << "null move added" << endl;
	}

	child->_parent_count++;
	ChildLink link;
	link._node = child;
	// Bug 2 model A (litteral, par-arete decouple) : sous DAG le compteur
	// d'arete ne derive JAMAIS du child->_nodes partage (qui s'auto-inflate
	// en cascade sur un noeud multi-parent -> overflow int -> N negatif/
	// milliards/aleatoire). Il part de 0 et comptera les iterations routees
	// par CETTE arete. OFF : arbre inchange (= child->_nodes) -> byte-identique.
	link._propagated_nodes = g_tt_node_dag ? 0 : child->_nodes;
	_children[move] = link;
	//_nodes += child->_nodes;
}

// Fonction qui renvoie le nombre de fils
size_t Node::children_count() const {
	return _children.size();
}

// Fonction qui renvoie le premier coup qui n'a pas encore été ajouté
Move Node::get_first_unexplored_move(bool fully_explored) {
	for (int i = 0; i < _board->_got_moves; i++) {
		Move move = _board->_moves[i];
		if (!_children.contains(move) || (fully_explored && _children[move]._node != nullptr && !_children[move]._node->_fully_explored)) {
			return move;
		}
	}

	return Move();
}

// Initie le noeud en fonction de son plateau
void Node::init_node() {

	if (_initialized) {
		return;
	}

	_initialized = true;

	if (_board == nullptr) {
		cout << "null board in init_node" << endl;
		return;
	}

	_board->get_moves();
	_board->is_game_over();

	// Si la partie est finie
	if (_board->_game_over_value != unterminated) {
		_fully_explored = true;
		_can_explore = false;
		_is_terminal = true;

		return;
	}

	// Génère et trie les coups
	_board->assign_all_move_flags();
	_board->sort_moves();
}

// #11 Plan B §7 — garde-fou anti-runaway. Constante haute (> toute profondeur
// reelle plausible) : ne se declenche QUE sur une recursion pathologique
// non-repetition (la repetition est deja coupee par le recheck Task 3).
// File-local (moteur mono-thread) ; defini avant grogros_zero (usage recursif).
static int g_dag_recursion_depth = 0;
constexpr int DAG_MAX_RECURSION_DEPTH = 1024;

// #11 Plan B — compteurs de diagnostic (toggle-gated, cumulatifs depuis le
// dernier reset GUI). Servent a voir CE QUI SE PASSE (spin ? partage ?
// recursion profonde ?) sans deviner. Lus par dag_debug_report.
static long long g_dag_recheck_hits = 0;  // §3 : repetitions path-locales coupees
static long long g_dag_link_hits = 0;     // link-on-create : noeud partage reutilise
static long long g_dag_link_misses = 0;   // link-on-create : nouveau noeud cree
static long long g_dag_variant_cuts = 0;  // get_exploration_variants : lignes coupees sur cycle
static int g_dag_max_recursion_seen = 0;  // pic de profondeur de recursion grogros_zero

// Detail borne PAR BATCH (remis a zero par dag_debug_report). On veut VOIR
// les premiers evenements (quel coup partage, quel cycle, quelle eval) sans
// inonder la console ni payer to_fen() des millions de fois sur le chemin le
// plus chaud (perf #1). dag_dbg_take() renvoie true au plus DAG_DBG_MAX fois
// par batch ; appele uniquement derriere une garde g_tt_node_dag deja vraie.
static int g_dag_dbg_emitted = 0;
constexpr int DAG_DBG_MAX = 40;

// Interrupteur COMPILE-TIME du diagnostic. false -> dag_dbg_take() renvoie
// false a la compilation, donc tous les blocs `if (dag_dbg_take()) cout...`
// (et leurs to_fen()) ET dag_debug_report() sont du code mort elimine : zero
// surcout sur la recherche. Repasser a true pour re-instrumenter (ex: travail
// Bug 1 opt 1). Les compteurs g_dag_* restent (1 inc, negligeable, jamais
// imprimes quand off).
constexpr bool dag_debug = false;

static bool dag_dbg_take() {
	if constexpr (!dag_debug) return false;
	if (g_dag_dbg_emitted >= DAG_DBG_MAX) return false;
	g_dag_dbg_emitted++;
	return true;
}

void dag_debug_report() {
	if constexpr (!dag_debug) return; // zero surcout quand le diagnostic est off
	cout << "[DAG] node_map=" << node_map.size()
	     << " link_hit=" << g_dag_link_hits
	     << " link_miss=" << g_dag_link_misses
	     << " recheck=" << g_dag_recheck_hits
	     << " variant_cut=" << g_dag_variant_cuts
	     << " max_rec=" << g_dag_max_recursion_seen
	     << " (detail=" << g_dag_dbg_emitted << "/" << DAG_DBG_MAX << ")" << endl;
	g_dag_dbg_emitted = 0; // fenetre de detail fraiche au prochain batch
}

// Nouveau GrogrosZero
void Node::grogros_zero(BoardBuffer* board_buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int iterations, int quiescence_depth, Network* network, PositionHistory *path_history, Evaluation* path_local_eval) {
	// TODO:
	// On peut rajouter la profondeur
	// Garder le temps de calcul

	// BUG: rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1
	// quiescence là dessus, après il regarde à donf Tb1 après Dxh7... ??

	// FIXME *** cela ne devrait pas arriver
	if (iterations <= 0) {
		cout << "iterations <= 0 in grogros_zero" << endl;
		return;
	}

	// #11 Plan B §7 — borne de securite sur la profondeur de recursion DAG.
	// OFF : jamais arme (ni compteur ni test). ON : la repetition (Task 3)
	// coupe deja tout cycle ; ceci n'attrape qu'une recursion pathologique.
	if (g_tt_node_dag && g_dag_recursion_depth >= DAG_MAX_RECURSION_DEPTH) {
		return;
	}
	g_dag_recursion_depth += g_tt_node_dag ? 1 : 0;
	if (g_tt_node_dag && g_dag_recursion_depth > g_dag_max_recursion_seen) {
		g_dag_max_recursion_seen = g_dag_recursion_depth;
	}
	struct DagRecGuard {
		~DagRecGuard() { g_dag_recursion_depth -= g_tt_node_dag ? 1 : 0; }
	} _dag_rec_guard;

	// Temps de calcul
	const clock_t begin_monte_time = clock();

	// #7 / B-1 — un seul historique possédé à la racine, threadé par pointeur.
	// Plus de clone par itération : l'isolement inter-itérations est garanti
	// par le push/pop équilibré (PathScope) dans explore_new_move /
	// explore_random_child — chaque itération restitue l'historique à cet état.
	PositionHistory local_path_history;
	PositionHistory* base_path_history = path_history != nullptr ? path_history : &local_path_history;
	ensure_position_in_history(*base_path_history, *_board);
	_board->get_zobrist_key();

	// INITIALISATION
	if (!_initialized) {
		quiescence(board_buffer, eval, quiescence_depth, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, base_path_history);
		_iterations++;
	}

	// Si la partie est finie, on ne fait rien
	if (_is_terminal) {
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// #11 Plan A — feuille TT gelée : jamais ré-évaluée ni étendue.
	// OFF-safe : tout _can_explore=false existant est aussi _is_terminal
	// (déjà capté ci-dessus) ; ne se déclenche que pour la feuille TT (ON).
	if (!_can_explore) {
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// FIXME *** cela ne devrait pas arriver
	if (_board->_got_moves <= 0) {
		cout << "no moves in grogros_zero" << endl;
		return;
	}

	// #11 Plan B — Bug 1 opt 3 : exclusion per-traversal partagee par TOUTES
	// les iterations de CET appel grogros_zero (vit sur la pile de ce frame
	// uniquement, jamais sur un noeud/arete partages). OFF : passee nullptr,
	// jamais consultee -> comportement byte-identique a l'arbre.
	DagExcl dag_excl;

	// Exploration
	while (iterations > 0) {

		// Si les buffers sont pleins, on n'étend plus : on raffine l'arbre existant.
		const bool can_expand = !monte_board_buffer.is_full() && !monte_node_buffer.is_full();

		// EXPLORATION D'UN NOUVEAU COUP
		if (can_expand && get_fully_explored_children_count() < _board->_got_moves) {
			explore_new_move(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history);
		}

		// EXPLORATION D'UN COUP DÉJÀ EXPLORÉ (raffinage)
		else if (children_count() > 0) {
			explore_random_child(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history, g_tt_node_dag ? &dag_excl : nullptr);
		}

		// Buffers pleins ET rien à raffiner ici : arrêt propre + log unique
		else {
			if (!g_buffers_full_logged) {
				cout << "buffer plein - arbre plafonne a " << _nodes
				     << " noeuds, on continue a raffiner l'existant" << endl;
				g_buffers_full_logged = true;
			}
			break;
		}

		iterations--;
	}
	
	// FIXME *** cela ne devrait pas arriver. Sous DAG, _nodes est une borne
	// proxy par-arete (Bug 2 model A, clamp >=0) : garde arbre silencee (Task 4).
	if (!g_tt_node_dag && _nodes <= 0) {
		cout << "negative nodes in grogros zero???" << endl;
	}

	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	return;
}

// Fonction qui explore un nouveau coup
void Node::explore_new_move(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network, PositionHistory *path_history, Evaluation* path_local_eval) {

	// On prend le premier coup non exploré
	const Move move = get_first_unexplored_move(true);
	// #7 / B-1 — historique unique threadé (plus de copie par coup).
	PositionHistory& branch_history = *path_history;

	// Noeud fils
	Node *child = nullptr;

	// #11 Plan A — vrai pour le seul cas « nouveau noeud, non nulle » :
	// seul cas où la feuille TT peut court-circuiter la quiescence.
	bool created_new_node = false;

	// Si on a déjà exploré ce coup, mais pas complètement
	bool already_explored = _children.contains(move);
	ChildLink* child_link = already_explored ? &_children[move] : nullptr;

	if (already_explored) {
		child = child_link->_node;

		if (!g_tt_node_dag && _nodes <= child_link->_propagated_nodes) {
			cout << "child nodes >= nodes???" << endl; // tree-only : faux sous DAG (sous-arbre partage multi-parent)
		}

		// Bug 2 model A : sous DAG on ne pre-soustrait pas (le delta clamp >=0
		// en fin de fonction nette correctement, sans underflow).
		if (!g_tt_node_dag) {
			_nodes -= child_link->_propagated_nodes;
		}
	}
	else {
		// Prend une place dans le buffer
		Board* new_board = board_buffer->get_first_free_board();

		if (new_board == nullptr)
			return;

		new_board->copy_data(*_board, false, true);
		new_board->_is_active = true;
		new_board->make_move(move, false, true);

		// Si la position est déjà présente dans l'historique du chemin, on considère que c'est une nulle.
		// Cette information est propre au chemin courant; elle ne doit pas vivre dans les noeuds parents.
		if (position_is_draw_by_repetition(branch_history, *new_board)) {
			// 3Q2k1/5p1p/2p1p3/2p1P1pq/5P2/4K3/6PP/2r5 b - - 1 4 : position test
			// 6kr/4K2p/7B/3bN3/8/8/8/8 b - - 19 10 : bugs dans position test

			// Création du noeud fils
			child = monte_node_buffer.get_first_free_node();

			if (child == nullptr)
				return;

			init_terminal_draw_child(child, new_board, eval, network);
			child->_nodes = 1;
			child->_iterations = 1;
		}

		// Sinon, on crée un nouveau noeud normalement
		else {
			new_board->get_zobrist_key();

			// #11 Plan B — link-on-create. Si une position de meme cle Zobrist
			// est deja un Node VIVANT, on lie l'arete a ce Node partage au lieu
			// de recreer un sous-arbre. La branche nulle (au-dessus) fabrique
			// toujours une feuille distincte — jamais partagee. OFF : saute tout.
			Node* shared = nullptr;
			if (g_tt_node_dag) {
				const auto it = node_map.find(new_board->_zobrist_key);
				if (it != node_map.end()) {
					shared = it->second;
				}
			}

			if (shared != nullptr) {
				// Hit : pas d'allocation. On rend le board pris au buffer et on
				// pointe vers le Node existant ; add_child (plus bas) lie
				// l'arete et incremente shared->_parent_count. created_new_node
				// reste false → pas de probe TT Plan A sur un noeud partage.
				if (new_board->_buffer_index >= 0 && !monte_board_buffer._bulk_resetting) {
					monte_board_buffer.free_index(new_board->_buffer_index);
				}
				child = shared;
				g_dag_link_hits++;
				// Detail borne : QUELLE position est reutilisee, par combien de
				// parents (avant l'incrément add_child). Confirme le partage reel.
				if (dag_dbg_take()) {
					cout << "[DAG] link-hit key=" << std::hex << shared->_board->_zobrist_key
					     << std::dec << " pc=" << shared->_parent_count
					     << " nodes=" << shared->_nodes
					     << " @ " << shared->_board->to_fen() << endl;
				}
			}
			else {
				// Miss : création normale + enregistrement dans node_map.
				child = monte_node_buffer.get_first_free_node();

				if (child == nullptr)
					return;

				child->_board = new_board;
				created_new_node = true;

				if (g_tt_node_dag) {
					node_map[new_board->_zobrist_key] = child;
					g_dag_link_misses++;
				}
			}
		}
	}

    if (child == nullptr) {
        cout << "null child and shouldn't be (explore_new_move)" << endl;
        return;
    }

	// #11 Plan A — Probe TT (toggle OFF par défaut). Seulement sur un noeud
	// fraîchement créé non-nulle : les nulles par répétition sont path-dependent
	// (déjà court-circuitées plus haut) et ne doivent jamais être lues comme une
	// valeur de position. Bornes (STANDPAT/BETA/ALPHA) jamais réutilisées comme
	// valeur de noeud (sémantique de borne sans fenêtre en MCTS). On exige
	// _depth dans la bande write-back : un vrai sous-arbre raffiné, pas une
	// feuille de quiescence. NB : on lit child->_board (et non new_board, hors
	// portée ici car déclaré dans la branche else) ; created_new_node garantit
	// child->_board == le new_board fraîchement créé, zobrist déjà calculée.
	if (g_tt_main_search && created_new_node) {
		const ZobristEntry* tt_entry = transposition_table.probe(child->_board->_zobrist_key);
		if (tt_entry != nullptr
			&& tt_entry->_flag == TT_EXACT
			&& tt_entry->_depth >= QDEPTH_BAND + MIN_REUSE_LOG2) {
			const int tt_eval = tt_denormalize_mate(tt_entry->_eval, child->_board->_moves_count); // #3
			init_tt_leaf_child(child, child->_board, eval, network, tt_eval * child->_board->get_color());
		}
	}

	if (!child->_fully_explored) {

		bool test = false;

		PathScope _ps(branch_history, *child->_board); // push child position (popped on scope exit, all paths)

		if (test) {
			child->quiescence(board_buffer, eval, 2, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &branch_history); // TODO *** faire un cutoff plus facile, si l'éval de base est déjà mauvaise? par rapport à l'évaluation statique
		}

		// Si l'évaluation est meilleure que celle de base, on regarde la quiescence
		else if (!test || child->_static_evaluation._value * _board->get_color() > _static_evaluation._value * _board->get_color()) {

			child->quiescence(board_buffer, eval, quiescence_depth, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &branch_history);
		}

		child->_fully_explored = true;
	}

	// rnb1kbnr/ppp1pppp/2q5/1B6/8/2N5/PPPP1PPP/R1BQK1NR b KQkq - 3 4
	// rnb1kbnr/ppp1pppp/2q5/8/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4 : ici Fb5 -> +114 au lieu de +895

	// Augmente le nombre de noeuds. Bug 2 model A : sous DAG on n'absorbe PAS
	// child->_nodes (qui peut etre un sous-arbre PARTAGE cree par un autre
	// parent -> desync puis underflow negatif). L'accumulation par-arete
	// clamp >=0 est faite plus bas (reseed propagated). OFF : ligne arbre
	// inchangee -> byte-identique.
	if (!g_tt_node_dag) {
		_nodes += child->_nodes;
	}

	if (!g_tt_node_dag && _nodes <= 0) {
		cout << "negative nodes in explore_new_move???" << endl;
	}

	if (!g_tt_node_dag && child->_nodes <= 0) {
		cout << "negative nodes in explore_new_move child???" << endl;
	}

	if (child->_iterations == 0) {
		child->_iterations = 1;
	}

	// Ajoute le fils
	if (!already_explored) {
		add_child(child, move);
		child_link = &_children.at(move);
	}

	if (child_link->_chosen_iterations == 0) {
		child_link->_chosen_iterations = 1;
	}

	// Bug 2 model A — comptabilite _nodes par-arete sous DAG : delta clamp >=0,
	// jamais negatif, jamais reinitialise depuis un child->_nodes partage
	// (baseline = _propagated_nodes precedent ; arete neuve link-on-create :
	// add_child a pose baseline=child->_nodes -> delta 0, on n'absorbe pas le
	// sous-arbre etranger). OFF : reseed arbre inchange -> byte-identique.
	if (g_tt_node_dag) {
		// model A litteral : +1 unite de travail routee par cette arete.
		// Borne par le budget d'iterations -> jamais d'overflow ; totalement
		// decouple du child->_nodes partage auto-inflant -> plus de cascade.
		// _nodes devient un proxy "visites" borne (spec §6 ; non lu par la
		// selection ni le budget).
		_nodes += 1;
		child_link->_propagated_nodes += 1;
	}
	else {
		child_link->_propagated_nodes = child->_nodes;
	}
	_iterations += child->_iterations;

	// Tous les coups ont-ils déjà été explorés?
	bool all_moves_explored = (get_fully_explored_children_count()) == _board->_got_moves;

	// TEST: R3r1k1/1P3p2/3p2p1/5n2/4rb2/2P1p2P/4N3/2BK4 b - - 1 35
	//2R1r1k1/1P3p2/3p2p1/5nb1/4r3/2P1p2P/4N3/2BK4 b - - 3 36

	// TESTS: Q7/4p2p/3k4/5p2/4q3/6P1/P2PPK2/8 w - - 2 39

	// Tous les coups ont été explorés, donc on met à jour l'évaluation du plateau avec le meilleur coup
	Move best_move = get_best_score_move(alpha, beta, !all_moves_explored);

	// Standpat = le meilleur
	if (best_move.is_null_move()) {
		_is_stand_pat_eval = true;
	}
	else {
		_is_stand_pat_eval = false;
		_deep_evaluation = _children[best_move]._node->_deep_evaluation;

		// #11 Plan A — write-back de la valeur raffinée (le levier réel).
		// Garde : toggle ON, pas terminal (exclut les nulles path-dependent),
		// pas stand-pat (borne inf déjà gérée en TT_STANDPAT par la quiescence),
		// éval évaluée. Profondeur-proxy au-dessus de la bande quiescence.
		if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated) {
			transposition_table.store(_board->_zobrist_key,
				// side-to-move comme les stores quiescence (stand_pat = _value*color,
				// exploration.cpp:902/912) : sinon signe inverse pour les Noirs.
				tt_normalize_mate(_deep_evaluation._value * _board->get_color(), _board->_moves_count), // #3
				tt_writeback_depth(_nodes), TT_EXACT);
		}
	}

	// FIXME: si on a regardé tous les fils, et qu'aucun des coups n'améliore l'évaluation, on fait quoi?
	// - Option 1: on garde l'évaluation sans aucun coup
	// - Option 2: on garde l'évaluation du meilleur coup
	// Est-ce vraiment grave? peut-être pas car si on continue une profondeur plus loin, explore_random_child() va prendre le meilleur coup
}

// Fonction qui explore dans un plateau fils pseudo-aléatoire
void Node::explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network, PositionHistory *path_history, DagExcl* dag_excl, Evaluation* path_local_eval) {

	// Prend un fils aléatoire
	const Move move = pick_random_child(alpha, beta, gamma, dag_excl);

	// Bug 1 opt 3 — toutes les aretes explorables ont ete §3-exclues sur ce
	// chemin : plus rien de non-cyclique a raffiner ici cette iteration. On
	// compte l'iteration (comme la coupe §3) et on sort, sans spin ni acces
	// _children[null]. OFF : pick ne renvoie jamais null par cette voie.
	if (g_tt_node_dag && move.is_null_move()) {
		_iterations++;
		return;
	}

	ChildLink& child_link = _children[move];
	Node *child = child_link._node;
	// #7 / B-1 — historique unique threadé ; push de la position du fils pour
	// la durée de la récursion uniquement, pop garanti à la sortie de scope.
	PositionHistory& branch_history = *path_history;

	if (!g_tt_node_dag && child_link._propagated_nodes >= _nodes) {
		cout << "child nodes >= nodes in random exploration??? main position: " << _board->to_fen() << ", child position: " << child->_board->to_fen() << endl; // tree-only : faux sous DAG
	}

	// Nombre de noeuds du fils
	const int initial_child_nodes = child_link._propagated_nodes;

	// #11 Plan B §3 — soundness du DAG. Le sous-arbre du fils peut etre PARTAGE
	// (cree via un chemin A, descendu ici via un chemin B). Le statut de nulle
	// par repetition est PATH-LOCAL : il n'est ni dans le Node ni dans l'arete,
	// tous deux PARTAGES. On le re-derive contre l'historique du chemin COURANT.
	// Si nulle sur ce chemin : on coupe le cycle en NE descendant PAS dans le
	// sous-arbre partage. On ne mute RIEN de partage (ni child->_deep_evaluation,
	// ni this->_deep_evaluation, ni child_link, ni node_map) : ecrire une nulle
	// path-locale sur une structure partagee corromprait l'autre chemin (c'etait
	// le bug d'oscillation). Conservatif : l'iteration est comptee, la branche
	// cyclique simplement non approfondie ; le backup normal du parent (plus
	// bas, via ses autres fils) reste l'autorite. La remontee complete de la
	// valeur de nulle path-locale (spec §3) est un raffinement suivant, a
	// decider sur mesure si la profondeur mesuree le justifie. OFF : saute
	// (comportement arbre actuel — explore_random_child ne re-testait jamais
	// la repetition sur un fils existant).
	if (g_tt_node_dag && position_is_draw_by_repetition(branch_history, *child->_board)) {
		g_dag_recheck_hits++;
		// Detail borne — LA frontiere cle des "incoherences d'eval". On coupe le
		// cycle (return) AVANT le backup parent (:686) : cette iteration ne
		// rafraichit pas this->_deep_evaluation et l'arete cyclique n'est PAS
		// devaluee (spec §3 §"valeur de nulle path-locale" non implementee). Si
		// la meme arete revient en boucle ici => spin + eval parent figee/
		// incoherente selon le chemin. C'est l'evidence a confirmer.
		if (dag_dbg_take()) {
			cout << "[DAG] §3-cut child_key=" << std::hex << child->_board->_zobrist_key
			     << std::dec << " child_pc=" << child->_parent_count
			     << " parent_eval=" << _deep_evaluation._value
			     << " (fige, edge non devaluee)\n"
			     << "      parent=" << _board->to_fen() << "\n"
			     << "      child =" << child->_board->to_fen() << endl;
		}
		// Bug 1 opt 3 — anti-spin : memorise l'arete cyclique pour qu'elle ne
		// soit PAS re-selectionnee par les iterations restantes de cet appel
		// grogros_zero (liste sur la pile, jamais sur structure partagee :
		// invariant 772183a respecte ; ne mute toujours RIEN de partage).
		// Sans liste (OFF / debordement) : coupe conservatrice inchangee.
		if (dag_excl != nullptr) dag_excl->add(move);
		_iterations++;
		return;
	}

	// Explore le fils
	{
		PathScope _ps(branch_history, *child->_board);
		child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &branch_history); // L'évaluation du fils est mise à jour ici
	}

	// Met à jour l'évaluation du plateau avec le meilleur coup
	_deep_evaluation = _children[get_best_score_move(alpha, beta)]._node->_deep_evaluation;

	// #11 Plan A — write-back de la valeur raffinée (le levier réel).
	// Mêmes gardes que dans explore_new_move.
	if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated) {
		transposition_table.store(_board->_zobrist_key,
			// side-to-move comme les stores quiescence (stand_pat = _value*color,
			// exploration.cpp:902/912) : sinon signe inverse pour les Noirs.
			tt_normalize_mate(_deep_evaluation._value * _board->get_color(), _board->_moves_count), // #3
			tt_writeback_depth(_nodes), TT_EXACT);
	}

	// Augmente le nombre de noeuds. Bug 2 model A : sous DAG, delta par-arete
	// clamp >=0 (initial_child_nodes du chemin arbre ignore ; un fils PARTAGE
	// peut retrecir via un autre chemin/reset -> le delta arbre underflow
	// negatif). OFF : delta arbre inchange -> byte-identique.
	if (g_tt_node_dag) {
		// model A litteral : +1 (idem explore_new_move). Borne, decouple du
		// child->_nodes partage -> plus de cascade/overflow, N stable.
		_nodes += 1;
		child_link._propagated_nodes += 1;
	}
	else {
		_nodes += child->_nodes - initial_child_nodes;
		child_link._propagated_nodes = child->_nodes;
	}

	if (!g_tt_node_dag && _nodes <= 0) {
		cout << "negative nodes in explore_random_child???" << endl;
	}

	if (!g_tt_node_dag && child->_nodes <= 0) {
		cout << "negative nodes in explore_random_child child???" << endl;
	}

	// Augmente le nombre d'itérations
	_iterations++;
}

// Fonction qui renvoie le fils le plus exploré
Move Node::get_most_explored_child_move() {
	int max = -1;

	// Tri simple, on ne départage pas les égalités
	Move best_move = Move();

	for (auto const& [move, child_link] : _children) {
		if (child_link._chosen_iterations > max) {
			max = child_link._chosen_iterations;
			best_move = move;
		}
	}

	return best_move;
}

// Recyclage free-list d'un noeud detache + son plateau (spec §5).
// Garde : index dans le buffer (>=0) et pas pendant un reset global.
void recycle_detached_node(Node* node) {
	if (node == nullptr)
		return;

	// #11 Plan B — un noeud detache et recycle ne doit plus etre joignable via
	// node_map (sinon pointeur pendant / resurrection). On efface l'entree
	// seulement si elle pointe bien vers CE noeud (un miss a pu la reecrire).
	if (g_tt_node_dag && node->_board != nullptr) {
		const auto it = node_map.find(node->_board->_zobrist_key);
		if (it != node_map.end() && it->second == node) {
			node_map.erase(it);
		}
	}

	Board* board = node->_board;
	if (board != nullptr && board->_buffer_index >= 0 && !monte_board_buffer._bulk_resetting)
		monte_board_buffer.free_index(board->_buffer_index);

	if (node->_buffer_index >= 0 && !monte_node_buffer._bulk_resetting)
		monte_node_buffer.free_index(node->_buffer_index);
}

// Reset le noeud et ses enfants, et les supprime tous
void Node::reset(bool recursive) {
	_latest_first_move_explored = -1;
	_nodes = 0;
	_iterations = 0;
	_parent_count = 0;

	if (_board != nullptr) {
		_board->reset_board();
	}

	_initialized = false;
	_is_terminal = false;
	_can_explore = true;
	_time_spent = 0;
	_fully_explored = false;
	_static_evaluation.reset();
	_deep_evaluation.reset();
	_quiescence_depth = 0;
	_is_active = false;
	_is_stand_pat_eval = true;

	if (recursive) {
		for (auto const& [_, child_link] : _children) {
			if (child_link._node == nullptr) {
				continue;
			}

			child_link._node->_parent_count--;

			if (child_link._node->_parent_count <= 0) {
				child_link._node->reset(true);
				// Approche B : l'enfant est definitivement detache -> on le
				// recycle (lui + son plateau). JAMAIS `this` (reuse en place).
				recycle_detached_node(child_link._node);
			}
		}
	}

	_children.clear();
}

// Fonction qui renvoie les variantes d'exploration
string Node::get_exploration_variants(const double alpha, const double beta, bool main, bool quiescence, int max_depth, PositionHistory* chain) {

	// Protection contre les cycles de transposition
	if (max_depth <= 0) {
		return "...";
	}

	// Si on est en fin de variante
	if (_board->_game_over_value) {
		return "";
	}

	// #11 Plan B — stoppe la ligne affichee sur une transposition/repetition.
	// Sous DAG _children est un graphe et peut reboucler : sans ceci la variante
	// se deroule jusqu'au cap max_depth (500) -> variantes geantes + GUI tres
	// lente (rafraichissement toutes les ~3-4 s). Idiome identique a
	// get_main_depth ; une chaine PAR ligne (copie par fils au noeud principal,
	// threadee le long d'une ligne unique). Sans DAG aucune cle ne se repete
	// -> affichage strictement inchange.
	PositionHistory local_chain;
	PositionHistory* c = chain != nullptr ? chain : &local_chain;
	_board->get_zobrist_key();
	// Coupe a la nulle reelle : triple (FIDE) sous DAG ; double = seuil arbre
	// historique quand OFF -> strictement byte-identique (count+1>=limite,
	// meme forme que position_is_draw_by_repetition).
	const auto _ch_it = c->find(_board->_zobrist_key);
	const int _ch_seen = _ch_it != c->end() ? static_cast<int>(_ch_it->second) : 0;
	const uint8_t _disp_limit = g_tt_node_dag ? display_repetition_limit : search_repetition_limit;
	if (_ch_seen + 1 >= _disp_limit) {
		// Detail borne : OU la ligne affichee reboucle (profondeur atteinte
		// avant le cycle). Quantifie le "variations quasi infinies" cote
		// affichage. g_tt_node_dag-gardé : OFF -> seuil arbre inchange.
		if (g_tt_node_dag) {
			g_dag_variant_cuts++;
			if (dag_dbg_take()) {
				cout << "[DAG] variant-cut depth_left=" << max_depth
				     << " key=" << std::hex << _board->_zobrist_key << std::dec
				     << " @ " << _board->to_fen() << endl;
			}
		}
		return "...";
	}
	(*c)[_board->_zobrist_key]++;

	string variants;

	// S'il y a des coups explorés
	if (children_count() > 0) {

		// Si on est dans le noeud principal, on affiche toutes les variantes
		if (main) {
			// TODO *** améliorer ce tri...

			// Trie les enfants par nombre d'itérations par l'algo de GrogrosZero
			// REVIEW *** voir si le vecteur est trop lent
			vector<pair<int, Move>> children_iterations;

			for (auto const& [move, child_link] : _children) {
				children_iterations.emplace_back(-child_link._chosen_iterations, move); // On met un moins pour trier dans l'ordre décroissant
			}

			std::ranges::sort(children_iterations.begin(), children_iterations.end());

			// TODO *** trier secondairement par score de coup

			for (auto const& [neg_child_iterations, move] : children_iterations) {
				Node* child = _children[move]._node;
				const int child_iterations = -neg_child_iterations;
				const int child_chosen_iterations = _children[move]._chosen_iterations;
				const bool new_quiescence = !quiescence && child_iterations == 0;

				string child_variants;
				child_variants.reserve(1024);

				char buf[128];

				// Ligne 1 : move + variant
				if (new_quiescence)
					child_variants += '(';

				child_variants += to_string(_board->_moves_count);
				child_variants += _board->_player ? ". " : "... ";
				child_variants += _board->move_label(move, true);
				child_variants += child->children_count() > 0 ? " " : "";
				PositionHistory child_chain = *c; // ligne independante : prefixe copie, pas de pollution entre fils
				child_variants += child->get_exploration_variants(alpha, beta, false, new_quiescence || quiescence, max_depth - 1, &child_chain);

				if (new_quiescence)
					child_variants += ')';

				child_variants += '\n';

				// Ligne 2 : Eval
				const int confidence = 100 - static_cast<int>(100.0 * child->_deep_evaluation._uncertainty);
				snprintf(buf, sizeof(buf), "Eval: %s (%d%%) | %s | Score: %s\n",
					_board->evaluation_to_string(child->_deep_evaluation._value).c_str(),
					confidence,
					child->_deep_evaluation._wdl.to_string().c_str(),
					score_string(child->_deep_evaluation._avg_score).c_str()
				);
				child_variants += buf;

				// Calculs de ratios
				const int nodes = _nodes;
				const int child_nodes = child->_nodes;
				const int nodes_ratio = nodes == 0 ? 0 : child_nodes * 100 / nodes;

				const int iterations = _iterations;
				const int iterations_ratio = iterations == 0 ? 0 : child_iterations * 100 / iterations;
				const int chosen_iterations_ratio = iterations == 0 ? 0 : child_chosen_iterations * 100 / iterations;

				if (nodes == 0) {
					cout << "nodes == 0?? le bug est peut-être ici..." << std::endl;
				}

				// Ligne 3 : stats
				snprintf(buf, sizeof(buf),
					"C: %s (%s%%) | I: %s (%s%%) | N: %s (%s%%) | D: %s | T: %s\n\n",
					int_to_round_string(child_chosen_iterations).c_str(),
					int_to_round_string(chosen_iterations_ratio).c_str(),
					int_to_round_string(child_iterations).c_str(),
					int_to_round_string(iterations_ratio).c_str(),
					int_to_round_string(child_nodes).c_str(),
					int_to_round_string(nodes_ratio).c_str(),
					int_to_round_string(child->get_main_depth(alpha, beta) + 1).c_str(),
					clock_to_string(child->_time_spent, true).c_str()
				);
				child_variants += buf;

				variants += child_variants;
			}

		}

		// Sinon, on affiche seulement le coup le plus exploré
		else {
			// Affiche seulement le premier coup (le plus exploré, et en cas d'égalité, celui avec la meilleure évaluation)
			const Move best_move = get_best_score_move(alpha, beta, true);

			// Standpat
			if (best_move.is_null_move()) {
				variants += "...";
			}
			else {
				Node* best_child = _children[best_move]._node;
				const bool new_quiescence = !quiescence && best_child->_iterations == 0;
				variants += (new_quiescence ? "(" : "") + (_board->_player ? to_string(_board->_moves_count) + ". " : "") + _board->move_label(best_move, true) + (best_child->children_count() > 0 ? " " : "") + best_child->get_exploration_variants(alpha, beta, false, new_quiescence || quiescence, max_depth - 1, c) + (new_quiescence ? ")" : "");
			}
		}
	}

	// S'il n'y a pas de coups explorés
	else {

		// On affiche l'évaluation du plateau en fin de variante
		variants = "";
	}
	
	return variants;
}

// Fonction qui renvoie la profondeur de la variante principale
int Node::get_main_depth(const double alpha, const double beta, int max_depth, PositionHistory* chain) {
	if (max_depth <= 0) {
		return 0;
	}

	if (children_count() > 0) {
		Move main_move = get_best_score_move(alpha, beta, true);

		if (main_move.is_null_move()) {
			return 0;
		}

		Node* main_child = _children[main_move]._node;

		if (main_child == nullptr) {
			return 0;
		}

		// #11 Plan B — stoppe la PV sur une transposition/repetition. Sous DAG,
		// la chaine du meilleur coup peut reboucler (graphe) : sans ceci
		// get_main_depth recurse jusqu'au cap max_depth=500 (oscillation
		// 500/peu profond observee). Idiome null-safe identique a grogros_zero :
		// le 1er appel possede l'historique de chaine. Sans DAG aucune cle ne
		// se repete -> meme resultat qu'avant (arbre au byte pres).
		PositionHistory local_chain;
		PositionHistory* c = chain != nullptr ? chain : &local_chain;
		_board->get_zobrist_key();
		(*c)[_board->_zobrist_key]++;
		main_child->_board->get_zobrist_key();
		// Meme seuil que get_exploration_variants : triple (FIDE) sous DAG,
		// double quand OFF -> profondeur affichee coherente avec le texte de
		// la variante et byte-identique a l'arbre quand OFF.
		const auto _mc_it = c->find(main_child->_board->_zobrist_key);
		const int _mc_seen = _mc_it != c->end() ? static_cast<int>(_mc_it->second) : 0;
		const uint8_t _md_limit = g_tt_node_dag ? display_repetition_limit : search_repetition_limit;
		if (_mc_seen + 1 >= _md_limit) {
			return 0; // la PV atteint la nulle par repetition -> fin de variante
		}

		return main_child->get_main_depth(alpha, beta, max_depth - 1, c) + 1;
	}

	return 0;
}

// Destructeur
Node::~Node() {
	//cout << "destructor not implemented" << endl;
	//for (int i = 0; i < children_count(); i++) {
	//	delete _children[i];
	//}
}

// Fonction qui renvoie le fils le plus exploré
Node* Node::get_most_explored_child() {
	Move most_explored_move = get_most_explored_child_move();

	if (most_explored_move.is_null_move()) {
		return nullptr;
	}

	return _children[most_explored_move]._node;
}

// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
int Node::get_avg_nps() const {
	return _time_spent == 0 ? 0 : ((double)_nodes / (double)_time_spent * CLOCKS_PER_SEC);
}

// Fonction qui renvoie le nombre d'itérations par seconde
int Node::get_ips() const {
	return _time_spent == 0 ? 0 : ((double)_iterations / (double)_time_spent * CLOCKS_PER_SEC);
}

// Quiescence search intégré à l'exploration
int Node::quiescence(BoardBuffer* board_buffer, Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha, int beta, Network* network, bool evaluate_threats, int beta_margin, PositionHistory *path_history) {
	// TODO: comment gérer la profondeur? faire en fonction de l'importance de la branche?
	// mettre aucune profondeur limite?
	// pourquoi en endgame ça va si loin? il fait full échecs...

	// Améliorations:
	// - Aucun cutoff quand on est en échec (DONE?)
	// - Marge pour les beta cutoffs (DONE?)
	// - SEE filtering -> profondeur réduite pour les mauvaises captures
	// - Delta pruning (DONE?)
	// - Futility pruning
	// - Traitement différent des échecs
	// - Standpat différent s'il y'a beaucoup de menaces adverses (voir test standpat = quiecsence adverse) (DONE?)
	// - Emergency cutoff (si on est vraiment trop profond...) (DONE?)
	// - Tri des captures: on regarde en priorité celles qui valent le plus le coup? (DONE?)
	// - Ne pas regarder les mauvaises captures? à voir si ça casse pas tout...
	// - Late move reduction (on réduit la profondeur pour les coups les moins prometteurs) (DONE?)
	// https://medwinpublishers.com/OAJDA/an-optimization-method-for-quiescence-in-chess-playing-automata.pdf : essayer de placer le meilleur coup calme en premier dans la liste des coups à explorer?
	// - Couper une variante si elle est beaucoup trop en dessous... (delta pruning?)
	// - Transpositions (beaucoup de positions sont déjà évaluées, on peut les réutiliser)
	// - Implémenter un générateur de coups spécifique pour la quiescence (seulement les prises par exemple, pour aller plus vite)
	// - Réduire la profondeur de la quiescence?
	// - Pruner plus agressivement si on est à faible profondeur restante?
	// - On peut paramétrer plus ou moins agressivement les facteurs de pruning (delta plus fort par exemple)
	// - Standpat pruning (voir dans la partie du l'alpha cut au niveau du standpat)
	// - Optimiser aussi la minimal_quiescence?
	// - Profondeur générale plus faible, et rendre plus profond quelques coups prometteurs?
	// - Remettre les échecs?
	// - History pruning
	// - Check extensions
	// - Trier les coups pour parer les échecs avec le meilleur en premier
	// - A la place des captures, mettre tous les coups qui augmentent l'éval d'un certain facteur? (et faire le tri par évaluation)?

	//1nb2rk1/r4ppp/p4q2/2p2N2/8/2bB1Q2/PPP2PPP/3RK2R w K - 0 18 : quiescence pourrie ici
	//r1b2rk1/1p1p2pp/p3p3/2P1p3/nR1K1P2/6B1/P5PP/5B1R w - - 0 3 : ici aussi
	// 2bk1r2/4b1Qp/8/1P6/3P4/1qp5/4NPPP/R1K2B1R b - - 0 25 : il voit pas le mat en 2?? Db2+ Rd1 Dd2#
	// 2brr2k/1ppqbppB/p6p/2PP4/1n6/4B2P/3N1PP1/RQ2R1K1 w - - 1 27 : pour mieux comprendre ce genre de positions, rajouter les hanging pieces?

	// YA DES BUGS DE PRUNING...
	//r1bqr2k/1pp2p1B/p3p2Q/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 3 26
	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : #2......
	//r1bqr1k1/1pp2p1Q/p3p1B1/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 b - - 6 27 ... ? il regarde pas plus loin?? il affiche pas #1 comme éval...

	// r4rk1/ppp2ppp/2nq1b2/2n5/2Pp4/2NBQ2P/PP1B1PP1/R3R1K1 w - - 0 16 : ??????????????
	//rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3 : ????
	// 4r3/4b1p1/p2B2k1/7p/1p4p1/1P6/P1P1RPP1/5K2 b - - 3 40 : pas de quiescence???
	// r1bqk2r/ppp2ppp/1b6/1P1nP3/2B5/5P2/P5PP/RNBQK1NR b KQkq - 0 10
	// FIX!! r3r3/P2k2pp/1Bb5/6N1/3P1n2/1Q1q4/PP3PPP/1KR5 w - - 3 27 : quiescence qui ne va pas jusqu'au bout? (peut-être car c'est moins bon que le standpat?)
	// 3k4/3p1p2/5P2/8/3qP2P/8/PrPQ1R2/R3K1r1 w Q - 4 32 : pareil, la quiescence?
	// 3k4/3p4/8/8/3q4/8/3Q1R2/4K1r1 w - - 4 32 : test
	// 3k4/3p1p2/5P2/8/3qP2P/8/PrPQ4/R4K2 b - - 0 33 : 0 quiescence ici?
	// 1r1q1r1k/2n4p/p2p1p1Q/2ppn3/7N/4R3/1PP1N1PP/5R1K w - - 1 24 : quiescence buggée après Txe5??????????
	// 1nb2rk1/r4ppp/p4q2/1Bp1bN2/8/2N2Q2/PPP2PPP/3RK2R w K - 1 17 : après Fxc3, ça va pas au bout des variantes...
	// 2rqr1k1/pNbnnpp1/2p1p1p1/P2pP3/Q2P4/B1P4P/4BPP1/RR4K1 b - - 6 22 : menace la dame en d8
	//rnb2bnr/ppp1pppp/2k1q3/8/8/3B4/PPPP1PPP/RNB1K1NR w KQ - 0 4
	// 1knr4/8/Qp3R1p/1N1r4/1P4p1/5P2/6PP/R6K b - - 0 36 : il regarde pas Td1+?
	// 3Q4/5pkp/2p1p3/2p1P1pq/5P2/4K3/6PP/2r5 w - - 2 5 : perpet TEST
	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : le check extension fonctionne pas?
	// 3Q2k1/5p1p/2p1p3/2p1P1pq/5P2/4K3/6PP/2r5 b - - 1 4 : position test nulle
	// 1r1q1r1k/2n4p/p2p1p1Q/2ppR3/7N/8/1PP1N1PP/5R1K b - - 0 24 : quiescence ici

	// On a au moins évalué le plateau du noeud
	if (_nodes <= 0) {
		_nodes = 1;
	}

	// Temps de calcul
	const clock_t begin_monte_time = clock();

	// Initialisation du noeud
	init_node();
	_board->get_zobrist_key();

	// #7 / B-1 — null-safe path history (appel manuel quiescence via main_gui.h) :
	// historique local possédé pour TOUTE la durée de l'appel. Doit être déclaré
	// au scope fonction (jamais par coup, sinon path_history pend sur le local
	// détruit de l'itération précédente). Idiome identique à grogros_zero.
	// Chemin recherche : path_history toujours non nul → no-op.
	PositionHistory local_path_history;
	if (path_history == nullptr) {
		path_history = &local_path_history;
	}

	// Couleur du joueur
	int color = _board->get_color();

	// Evalue la position
	if (!_static_evaluation._evaluated) {
		evaluate_position(eval, false, network, true);
	}
	else {
		_deep_evaluation = _static_evaluation;
	}

	_quiescence_depth = depth;

	// Si la partie est finie
	if (_is_terminal) {
		_nodes = 1;
		_iterations = 1;
		_time_spent += clock() - begin_monte_time;

		return _deep_evaluation._value * color;
	}

	// Stand pat
	const int stand_pat = _deep_evaluation._value * color;
	const int original_alpha = alpha;

	// TT Probe
	const ZobristEntry* tt_entry = transposition_table.probe(_board->_zobrist_key);
	if (tt_entry != nullptr && tt_entry->_depth >= depth) {
		const int tt_eval = tt_denormalize_mate(tt_entry->_eval, _board->_moves_count); // #3 : dé-canonise la distance de mat au _moves_count du nœud courant
		bool tt_cutoff = false;

		if (tt_entry->_flag == TT_EXACT) {
			_deep_evaluation._value = tt_eval * color;
			tt_cutoff = true;
		}
		else if ((tt_entry->_flag == TT_BETA || tt_entry->_flag == TT_STANDPAT) && tt_eval >= beta) {
			// TT_STANDPAT = borne inférieure statique : même consommation que TT_BETA
			// (cutoff seulement si tt_eval >= beta -> fail-high sain). Jamais de cutoff
			// "exact" fenêtre-indépendante sur une éval statique (BUGFIXES #4).
			_deep_evaluation._value = beta * color;
			tt_cutoff = true;
		}
		else if (tt_entry->_flag == TT_ALPHA && tt_eval <= alpha) {
			_deep_evaluation._value = alpha * color;
			tt_cutoff = true;
		}

		if (tt_cutoff) {
			// Le cutoff n'a fixé que _value. Les champs dérivés (_wdl, _avg_score)
			// venaient encore de l'éval statique : un parent héritant de cette
			// _deep_evaluation voyait une éval incohérente (ex: _value = mat mais
			// _avg_score d'une position normale), ce qui faussait get_node_score.
			// On les recalcule à partir du _value de la TT.
			// Si _value est un score de mat, get_WDL le remixe avec _uncertainty et
			// _winnable_* restés statiques (board.cpp:10061-10070) -> le score ne
			// converge pas vers 0/1 (ex: M4 affiché mais score 0.934, conf 87%).
			// On remet ces champs cohérents comme le chemin terminal (board.cpp:1575-1577).
			// #14 : une valeur issue d'un cutoff TT est une valeur *recherchée*
			// jugée fiable au point d'arrêter la recherche. Son score affiché
			// (_avg_score via get_WDL) doit dériver du _value de la TT, pas de
			// l'_uncertainty de l'éval *statique* restée en place (sinon value et
			// score divergent jusqu'à ré-exploration). On force _uncertainty=0
			// pour TOUT cutoff — généralisation du fix #1 v2 au cas non-mat,
			// cohérent avec les chemins terminal/mat/NN (board.cpp:1558/1575/1592).
			// #1 v2 / #14 : champs dérivés cohérents depuis le _value de la TT.
			// Logique factorisée dans tt_fixup_derived (réutilisée par la
			// feuille TT de la recherche principale, #11 Plan A).
			tt_fixup_derived(_deep_evaluation);

			transposition_table._stats._cutoffs++;
			_time_spent += clock() - begin_monte_time;
			if (tt_entry->_flag == TT_EXACT) return tt_eval;
			if (tt_entry->_flag == TT_ALPHA) return alpha;
			return beta; // TT_BETA / TT_STANDPAT : borne inférieure (fail-high) -> beta
		}
	}

	// Si on est en échec (pour ne pas terminer les variantes sur un échec)
	const bool in_check = _board->in_check();

	// Emergency cutoff: depth - 4
	if (depth <= -4) {
		transposition_table.store(_board->_zobrist_key, tt_normalize_mate(stand_pat, _board->_moves_count), depth, TT_STANDPAT); // #4 borne inf statique ; #3 mat canonisé
		_time_spent += clock() - begin_monte_time;
		cout << "emergency cutoff: " << _board->to_fen() << ", in_check: " << in_check << endl;
		return stand_pat;
	}

	// Profondeur nulle, on renvoie le standpat
	if (depth <= 0 && !in_check) {
		transposition_table.store(_board->_zobrist_key, tt_normalize_mate(stand_pat, _board->_moves_count), depth, TT_STANDPAT); // #4 borne inf statique ; #3 mat canonisé
		_time_spent += clock() - begin_monte_time;
		return stand_pat;
	}

	// Marge, qui dépend de la menace adverse (jugée grâce à la valeur de la quiescence sur le tour de l'adversaire)
	if (evaluate_threats && !in_check) {
		// Fait une courte quiescence pour évaluer la menace
		int new_stand_pat = -evaluate_quiescence_threat(eval, 2, search_alpha, search_beta, -INT32_MAX, INT32_MAX, network); // REVIEW *** faut-il faire une recherche plus profonde ?

		// Il faut aussi prendre en compte que le trait ayant changé de camp, il y a une petite variation d'évaluation due à cela...
		// Marge de 100 en moins
		new_stand_pat += 100;

		beta_margin = max(0, stand_pat - new_stand_pat);
	}

	// Si la branche est vraiment trop pourrie, on peut peut-être quand-même accepter un standpat
	double total_beta = in_check ? (double)beta + 500 : (double)beta + beta_margin;

	// FIXME *** normal qu'on envoie la beta margin sur toute la profondeur?

	if (stand_pat >= total_beta) {
		transposition_table.store(_board->_zobrist_key, tt_normalize_mate(beta, _board->_moves_count), depth, TT_BETA); // #3 mat canonisé
		_time_spent += clock() - begin_monte_time;
		//cout << "beta cutoff1: " << stand_pat << " >= " << beta << " + " << beta_margin << endl;
		return beta;
	}

	// TODO *** Test du standpat pruning (futility pruning)
	//constexpr int standpat_pruning_threshold = 1000;

	//if (stand_pat + standpat_pruning_threshold <= alpha)
	//	_time_spent += clock() - begin_monte_time;
	//	return stand_pat;

	// Mise à jour de alpha si l'éval statique est plus grande
	if (stand_pat > alpha && !in_check) {
		alpha = stand_pat;
	}

	int move_index = 0;

	// Regarde toutes les captures
	for (int i = 0; i < _board->_got_moves; i++) {

		// Coup
		const Move move = _board->_moves[i];
		
		// Déjà exploré ?
		bool already_explored = _children.contains(move);

		// *** FAUT-IL EXPLORER CE COUP? ***

		// Si on est en échec, on explore tous les coups
		const bool should_explore = in_check || move.is_capture() || move.is_promotion() || move.is_checkmate() || move.is_check();

		// *** EXPLORATION ***
		// Si c'est une capture/échec/promotion, on explore
		if (should_explore) {

			// Profondeur ajustée
			int new_depth = depth;

			// Prolongement de la profondeur en cas d'échec
			constexpr int check_extension = 0;
			new_depth += move.is_check() ? check_extension : 0;

			// Réduction de la profondeur pour les coups moins prometteurs
			new_depth -= in_check ? 0 : move_index * 2;

			if (new_depth <= 0 && !in_check) {
				continue; // On ne regarde plus les coups si on est trop profond
			}

			move_index++;
			
			// #7 / B-1 — historique unique threadé (plus de copie par coup).
			// La répétition reste correcte (clés Zobrist uniques par époque
			// réversible) ; push/pop équilibré via PathScope plus bas.
			PositionHistory& branch_history = *path_history;

			// Test du delta pruning
			if (!move.is_checkmate()) {

				constexpr int delta = 500;

				constexpr int piece_values[6] = { 100, 300, 300, 500, 900, 10000 }; // P, N, B, R, Q, K
				constexpr int promotion_value = 1000;
				constexpr int check_value = 500; // Valeur d'un échec

				// Estimation rapide de ce que le coup peut apporter
				int best_estimation = 0;

				// Si c'est une promotion, on ajoute la valeur de la promotion
				if (move.is_promotion()) {
					best_estimation += promotion_value;
				}

				// Si c'est une capture, on ajoute la valeur de la pièce capturée
				else if (move.is_capture()) {
					best_estimation += piece_values[(_board->_array[move.end_row][move.end_col] - 1) % 6];
				}

				// Si c'est un échec, on ajoute la valeur de l'échec
				else if (move.is_check()) {
					best_estimation += check_value;
				}

				if (stand_pat + best_estimation + delta < alpha) {
					continue; // On ne regarde pas ce coup
				}
			}

			// Création du noeud fils
			Node *child = nullptr;
			ChildLink* child_link = already_explored ? &_children[move] : nullptr;

			// Si on a déjà exploré ce coup, mais pas complètement
			if (already_explored) {
				child = child_link->_node;
				// position poussée pour la durée de la récursion par le PathScope ci-dessous
			}

			// On crée un nouveau noeud pour ce coup
			else {
				// Prend une place dans le buffer
				Board* new_board = board_buffer->get_first_free_board();

				// Buffer plein
				if (new_board == nullptr) {
					_time_spent += clock() - begin_monte_time;
					return alpha;
				}

				new_board->copy_data(*_board, false, true);
				new_board->_is_active = true;
				new_board->make_move(move, false, true);

				// Si la position est déjà présente dans l'historique, on considère que c'est une nulle.
				// Ici encore, l'information appartient au chemin courant et pas au noeud parent.
				if (position_is_draw_by_repetition(branch_history, *new_board)) {
					// Création du noeud fils
					child = monte_node_buffer.get_first_free_node();

					// Buffer plein
					if (child == nullptr) {
						_time_spent += clock() - begin_monte_time;
						return alpha;
					}

					init_terminal_draw_child(child, new_board, eval, network);
				}

				else {
					// position poussée pour la durée de la récursion par le PathScope ci-dessous
					new_board->get_zobrist_key();

					// Pas de partage via TT (même raison que explore_new_move)
					child = monte_node_buffer.get_first_free_node();

					// Buffer plein
					if (child == nullptr) {
						_time_spent += clock() - begin_monte_time;
						return alpha;
					}

					child->_board = new_board;
				}

				add_child(child, move);
				child_link = &_children[move];
			}

			// Appel récursif sur le fils — #7/B-1 : push la position du fils
			// pour la durée de la récursion, pop garanti à la sortie de scope.
			int score;
			{
				PathScope _ps(branch_history, *child->_board);
				score = - child->quiescence(board_buffer, eval, new_depth - 1, search_alpha, search_beta, -beta, -alpha, network, false, beta_margin, &branch_history);
			}
			const int previous_child_nodes = child_link != nullptr ? child_link->_propagated_nodes : 0;
			_nodes += child->_nodes - previous_child_nodes;
			if (child_link != nullptr) {
				child_link->_propagated_nodes = child->_nodes;
			}

			// Mise à jour de l'évaluation du plateau
			bool all_moves_explored = children_count() == _board->_got_moves;

			// Tous les coups ont été explorés, donc on met à jour l'évaluation du plateau avec le meilleur coup
			Move best_move = get_best_score_move(search_alpha, search_beta, !all_moves_explored, new_depth - 1);

			// Standpat = le meilleur
			if (best_move.is_null_move()) {
				//_is_stand_pat_eval = true;
			}
			else {
				//_is_stand_pat_eval = false;
				_deep_evaluation = _children[best_move]._node->_deep_evaluation;
				if (_children[best_move]._node->_quiescence_depth != new_depth - 1) {
					cout << "expected depth: " << new_depth - 1 << ", actual depth: " << _children[best_move]._node->_quiescence_depth << endl;
				}
			}

			// Beta cut-off
			if (score >= beta) {
				transposition_table.store(_board->_zobrist_key, tt_normalize_mate(beta, _board->_moves_count), depth, TT_BETA); // #3 mat canonisé
				_time_spent += clock() - begin_monte_time;
				return beta;
			}

			// Mise à jour de alpha si l'éval statique est plus grande
			if (score > alpha) {
				alpha = score;
			}
		}
	}

	// TT Store
	// BUGFIXES #4 : si la valeur finale est le plancher stand-pat (aucun fils ne l'a
	// dépassée -> alpha == stand_pat alors que alpha > original_alpha), ce n'est PAS
	// une valeur exacte recherchée mais une borne inférieure statique. La marquer
	// TT_STANDPAT (consommée comme TT_BETA) au lieu de TT_EXACT évite les faux cutoffs
	// fenêtre-indépendants (déblocage #11 plan A). En cas d'égalité exacte rare
	// fils==stand_pat, on dégrade TT_EXACT->TT_STANDPAT : conservatif donc toujours sain.
	TTFlag tt_flag;
	if (alpha <= original_alpha)
		tt_flag = TT_ALPHA;
	else if (alpha == stand_pat)
		tt_flag = TT_STANDPAT;
	else
		tt_flag = TT_EXACT;
	transposition_table.store(_board->_zobrist_key, tt_normalize_mate(alpha, _board->_moves_count), depth, tt_flag); // #3 mat canonisé

	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	return alpha;
}

// Fonction qui renvoie le nombre de noeuds fils complètement explorés
int Node::get_fully_explored_children_count() const {
	int count = 0;

	for (auto const& [_, child_link] : _children) {
		if (child_link._node->_fully_explored) {
			count++;
		}
	}

	return count;
}

// Fonction qui renvoie la somme des noeuds des fils
int Node::count_children_nodes() const {
	int sum = 0;

	for (auto& [move, child_link] : _children) {
		sum += child_link._node->get_total_nodes();
	}

	cout << "SUM: " << sum << endl;

	return sum;
}

// Fonction qui renvoie le nombre de noeuds total
// TODO: à utiliser seulement pour savoir si le buffer est plein
int Node::get_total_nodes() const {
	return count_children_nodes() + 1;
}

// Fonction qui évalue la position
void Node::evaluate_position(Evaluator* evaluator, bool display, Network * network, bool game_over_check, bool static_only) {
	_board->evaluate(&_static_evaluation, evaluator, display, network, game_over_check);

	if (!static_only) {
		_deep_evaluation = _static_evaluation;
	}
}

// Fonction qui renvoie un noeud fils pseudo-aléatoire (en fonction des évaluations et du nombre de noeuds)
Move Node::pick_random_child(const double alpha, const double beta, const double gamma, const DagExcl* dag_excl) {
	// TESTS
	// 8/8/8/1r5p/2p4k/2Kb4/8/8 b - - 1 69 : tout égal quand tout gagne...
	// r2qr1k1/3bbp1p/p2pn1p1/3QP3/3P4/3B1N2/1P1B1PPP/R3R1K1 w - - 1 24 : pareil
	// 3b2rk/3P2pp/8/p7/8/2Q1p3/PP1p1pPP/3RqR1K b - - 1 36 : il faut pas 100% de reflexion sur un coup, quand tous les coups gagnent
	// 8/8/8/1r5p/2p2k2/2Kb4/8/8 b - - 5 71 : pareil...
	// R3r1k1/1P3p2/3p2p1/5nb1/4r3/2P1p2P/4N3/2BK4 w - - 2 36 : il faut regarder Tc8 si toutes les réponses adverses sont pourries
	// r1r3k1/pp2bppp/3q4/3Pnp2/4nB2/2NB4/PP3PPP/R2QR1K1 w - - 5 16 : ???

	// Meilleur coup explorable
	Move move_to_play = Move();
	double explorable_best_score = -DBL_MAX;

	// Scores des coups

	int color = _board->get_color();

	// Meilleure valeur d'évaluation
	int max_eval = -INT_MAX;

	// Meilleure chance de gagner
	double max_avg_score = 0.0;

	for (auto const& [_, child_link] : _children) {
		Node* child = child_link._node;
		if (child->_deep_evaluation._value * color > max_eval) {
			max_eval = child->_deep_evaluation._value * color;
		}

		if (_board->_player ? child->_deep_evaluation._avg_score > max_avg_score : 1 - child->_deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? child->_deep_evaluation._avg_score : 1 - child->_deep_evaluation._avg_score;
		}
	}

	robin_map<Move, double> move_scores = get_move_scores(alpha, beta);

	struct ScoredMove {
		Move move;
		double score;
	};

	// Boost les valeurs de chaque coup en fonction de leur position
	static constexpr double boost_table[5] = { 25.0, 8.0, 4.0, 3.0,	2.0 };

	ScoredMove top[5];
	int top_count = 0;

	// Tri par insertion (fonction à implémenter)
	for (auto const& [move, score] : move_scores) {

		int j = top_count;
		if (j < 5) {
			top[j] = { move, score };
			++top_count;
		}
		else if (score <= top[j - 1].score) {
			continue;
		}

		while (j > 0 && top[j - 1].score < score) {
			if (j < 5)
				top[j] = top[j - 1];
			--j;
		}

		if (j < 5)
			top[j] = { move, score };
	}

	// Application du bonus
	for (int i = 0; i < top_count; ++i) {
		Move m = top[i].move;
		move_scores[m] = top[i].score * boost_table[i];
	}

	Move best_move;
	double best_score = 0.0;

	// Regarde chaque coup
	for (auto const& [move, child_link] : _children) {
		// Bug 1 opt 3 — arete §3-exclue sur ce chemin : ecartee de best_move
		// ET move_to_play, donc les iterations restantes de cet appel
		// grogros_zero ne spinnent plus dessus. OFF : dag_excl == nullptr
		// -> aucun effet -> byte-identique a l'arbre.
		if (dag_excl != nullptr && dag_excl->contains(move)) continue;
		Node* child = child_link._node;

		// Score du coup
		double move_score = move_scores[move];

		// Facteur d'exploration
		int child_iterations = max(child_link._chosen_iterations, child->_iterations);

		// Gamma
		const double new_gamma = gamma / (1.00f - _static_evaluation._uncertainty / 2.0f) / (1.00f - _board->_adv / 2.0f);

		// Exploration score
		double exploration_score = child_iterations == 0 ? _iterations * 2 : pow((double)_iterations / (double)child_iterations, new_gamma);

		// Score final
		double score = move_score * exploration_score;

		// rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2 : il doit garder Dh4 comme 99% de chosen, mais regarder les autres normalement...

		// Si tous les coups n'ont pas été regardés: augmente beaucoup le score
		//if (child->get_fully_explored_children_count() < child->_board->_got_moves) {
		//	score *= 10.0;
		//}

		// r2qk2r/1pp1b3/p3p3/n2P1bp1/4N2p/P3QP1B/1P6/2KR3R w kq - 1 21 : il faut régler ça...
		// R3r1k1/1P3p2/3p2p1/5nb1/4r3/2P1p2P/4N3/2BK4 w - - 2 36 : Tc8...

		// Tant qu'on a pas regardé tous les coups, on met un bonus
		if (child->_is_stand_pat_eval) {
			Evaluation best_eval = Evaluation();
			//Evaluation best_eval = child->_deep_evaluation;

			for (auto const& [move2, child_link2] : child->_children) {
				if (child->_board->_player ? (child_link2._node->_deep_evaluation > best_eval) : (child_link2._node->_deep_evaluation < best_eval)) {
					best_eval = child_link2._node->_deep_evaluation;
				}
			}

			// FIXME *** what to do if there no fully explored move?
			// FIXME *** should we just change the overall score to the potential eval we are getting here instead of a multiplier?

			//const float multiplier = best_eval._evaluated ? 1.0f + abs(best_eval._value - child->_deep_evaluation._value) / 0.000001f : 1.0f;

			//cout << "move: " << _board->move_label(move) << " | move_score: " << score << " | moves explored " << child->get_fully_explored_children_count() << "/" << (int)child->_board->_got_moves << " = score : " << score << " | stand pat : " << child->_deep_evaluation._value << " | best eval : " << (best_eval._evaluated ? to_string(best_eval._value) : "N/A") << " | multiplier : " << multiplier << endl;
			//cout << "move: " << _board->move_label(move) << " | move_score: " << score << " | stand pat : " << child->_deep_evaluation._value << " | best eval : " << (best_eval._evaluated ? to_string(best_eval._value) : "N/A") << endl;
			//score *= 10.0;
			//score *= multiplier;

			if (best_eval._evaluated) {
				if (best_eval._value * color > max_eval) {
					max_eval = best_eval._value * color;
				}

				if (_board->_player ? best_eval._avg_score > max_avg_score : 1 - best_eval._avg_score > max_avg_score) {
					max_avg_score = _board->_player ? best_eval._avg_score : 1 - best_eval._avg_score;
				}

				score = get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player, &best_eval) * exploration_score;
				//cout << "new score: " << score << endl;
			}
		}
		// Utiliser la différence d'évaluation entre les coups et le stand pat?

		//cout << "move: " << _board->move_label(move) << " | move_score: " << move_score << " | exploration_score : " << exploration_score << " | fully_explored : " << child->get_fully_explored_children_count() << " / " << child->children_count() << " = score : " << score << endl;

		// Si le score est meilleur
		if (score > best_score) {
			best_score = score;
			best_move = move;
		}

		// Si le coup est explorable
		if (child->_can_explore && score > explorable_best_score) {
			explorable_best_score = score;
			move_to_play = move;
		}
	}

	//cout << "best move: " << _board->move_label(best_move) << " | best score: " << best_score << endl << endl;

	// Bug 1 opt 3 — toutes les aretes explorables §3-exclues sur ce chemin :
	// aucun candidat. On signale "rien" (coup nul) ; explore_random_child
	// compte l'iteration et sort sans spin ni _children[null]. OFF : dag_excl
	// == nullptr -> ce cas ne se produit jamais, chemin arbre (cout + .at)
	// strictement inchange.
	if (best_move.is_null_move()) {
		if (dag_excl != nullptr) return Move();
		cout << "null move considered to be the best??" << endl;
	}

	// Meilleur coup global
	_children.at(best_move)._chosen_iterations++;

	if (move_to_play.is_null_move()) {
		return best_move;
	}

	return move_to_play;
}

// Fonction qui renvoie le score des coup
robin_map<Move, double> Node::get_move_scores(const double alpha, const double beta, const bool consider_standpat, const int qdepth) const {

	// Pour le standpat, on l'associe au null move

	// TEST: 8/8/8/1r5p/2p2k2/2Kb4/8/8 b - - 5 71

	int color = _board->get_color();

	// Meilleure valeur d'évaluation
	int max_eval = -INT_MAX;

	// Meilleure chance de gagner
	double max_avg_score = 0.0;

	// Cherche la meilleure eval et le meilleure score parmi tous les coups possibles
	for (auto const& [_, child_link] : _children) {
		Node* child = child_link._node;
		if (child->_deep_evaluation._value * color > max_eval) {
			max_eval = child->_deep_evaluation._value * color;
		}

		if (_board->_player ? child->_deep_evaluation._avg_score > max_avg_score : 1 - child->_deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? child->_deep_evaluation._avg_score : 1 - child->_deep_evaluation._avg_score;
		}
	}

	if (consider_standpat) {
		// Si le stand pat est meilleur que le meilleur coup
		if (_deep_evaluation._value * color > max_eval) {
			max_eval = _deep_evaluation._value * color;
		}
		if (_board->_player ? _deep_evaluation._avg_score > max_avg_score : 1 - _deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? _deep_evaluation._avg_score : 1 - _deep_evaluation._avg_score;
		}
	}

	robin_map<Move, double> move_scores;
	move_scores.reserve(children_count() + consider_standpat);

	// Valeur du stand pat
	if (consider_standpat) {
		move_scores.emplace(Move(), get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player));
	}

	// Regarde chaque coup
	for (auto const& [move, child_link] : _children) {
		Node* child = child_link._node;
		if (qdepth != -100 && child->_quiescence_depth != qdepth) {
			continue;
		}
		move_scores.emplace(move, child->get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player));
	}

	return move_scores;
}

// Fonction qui renvoie la valeur du noeud
double Node::get_node_score(const double alpha, const double beta, const int max_eval, const double max_avg_score, const bool player, Evaluation *custom_eval) const {

	const double min_constant = 1E-100;
	//const double add_constant = 0.05f;
	//const double add_constant = 5.0E-5;
	const double add_constant = 0.000f;
	//const double pure_win_chance_adding = 0.001f; // Bonus si on a des chances de gagner pures
	const double pure_win_chance_adding = 0.00025f; // Bonus si on a des chances de gagner pures

	int color = player ? 1 : -1;

	// Evaluation à utiliser
	Evaluation eval = custom_eval != nullptr ? *custom_eval : _deep_evaluation;

	// Facteur 1: évaluation
	double eval_score = eval._value * color;
	//int is_eval_mate = _board->is_eval_mate(_deep_evaluation._value) * color;
	
	//eval_score = (is_eval_mate == 0 ? exp(alpha * (eval_score - max_eval)) : 1.0f / (float)is_eval_mate) + min_constant;
	eval_score = exp(alpha * (eval_score - max_eval)) + min_constant;

	// Facteur 2: score moyen
	const double avg_score = player ? eval._avg_score : 1 - eval._avg_score;
	const double score_score = exp(-beta * (1 - avg_score) / (1 - max_avg_score + min_constant) * max_avg_score / (avg_score + min_constant)) + min_constant;

	// Bonus si t'as des chances pures de gain?
	// R4Q2/6pk/1q6/8/3P4/2N3r1/1K6/8 w - - 5 49 : ici pour trouver Ra2?
	// 8/6pk/7p/8/4p2P/1R1r2P1/5PK1/8 w - - 5 33 : Txd3?? ça gagne!
	//const float k_avg_score = 0.25f;
	const float k_avg_score = 0.25f;

	const double win_adding = ((player ? eval._wdl.win_chance : eval._wdl.lose_chance) + k_avg_score * avg_score) * pure_win_chance_adding;
	//const double win_adding = (player ? eval._wdl.win_chance : eval._wdl.lose_chance) * pure_win_chance_adding;
	//cout << "win chance: " << (player ? eval._wdl.win_chance : eval._wdl.lose_chance) << ", win_adding: " << win_adding << endl;

	//cout << "eval: " << eval_score << ", score: " << score_score << endl;

	// Facteur 3: rajout quasi constant? à tester...
	const double adding = (avg_score == 0.0f) ? 0.0f : avg_score / max_avg_score * add_constant;

	// Score final
	const double score = eval_score * score_score + adding + win_adding;
	//double score = eval_score * score_score + adding;

	//score *= (1.0f + win_adding);

	//cout << "Node score: " << score << " | eval: " << eval_score << ", score: " << score_score << ", adding: " << adding << ", win_adding: " << win_adding << endl;

	return score;
}

// Fonction qui renvoie le coup avec le meilleur score
Move Node::get_best_score_move(const double alpha, const double beta, const bool consider_standpat, const int qdepth) {

	int color = _board->get_color();

	// Meilleure valeur d'évaluation
	int max_eval = -INT_MAX;

	// Meilleure chance de gagner
	double max_avg_score = 0.0;

	// Cherche la meilleure eval et le meilleure score parmi tous les coups possibles
	for (auto const& [_, child_link] : _children) {
		Node* child = child_link._node;
		if (child->_deep_evaluation._value * color > max_eval) {
			max_eval = child->_deep_evaluation._value * color;
		}

		if (_board->_player ? child->_deep_evaluation._avg_score > max_avg_score : 1 - child->_deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? child->_deep_evaluation._avg_score : 1 - child->_deep_evaluation._avg_score;
		}
	}

	if (consider_standpat) {
		// Si le stand pat est meilleur que le meilleur coup
		if (_deep_evaluation._value * color > max_eval) {
			max_eval = _deep_evaluation._value * color;
		}
		if (_board->_player ? _deep_evaluation._avg_score > max_avg_score : 1 - _deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? _deep_evaluation._avg_score : 1 - _deep_evaluation._avg_score;
		}
	}


	// Meilleur coup
	Move best_move = Move();
	double best_score = -DBL_MAX;

	if (consider_standpat) {
		best_score = get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player);
		best_move = Move();
	}

	for (auto const& [move, child_link] : _children) {
		Node* child = child_link._node;
		if (qdepth != -100 && child->_quiescence_depth != qdepth) {
			continue;
		}
		double score = child->get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player);
		//cout << "move: " << _board->move_label(move) << " | score: " << score << endl;
		if (score > best_score || (best_move.is_null_move() && score == best_score)) {
			best_score = score;
			best_move = move;
		}
	}

	//cout << "best move: " << _board->move_label(best_move) << " | best score: " << best_score << endl;

	return best_move;
}

// Fonction qui renvoie une valeur prévisionnelle du score du noeud, lorsqu'on ne connait pas les évaluations max (pour la quiecence)
int Node::get_previsonal_node_score(const double alpha, const double beta, const bool player) const {
	return 1E6 * get_node_score(alpha, beta, _deep_evaluation._value, _deep_evaluation._avg_score, player);
}

// Fonction qui évalue la menace en utilisant une quiesence sur le tour de l'adversaire
int Node::evaluate_quiescence_threat(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha, int beta, Network* network) const {

	Board b(*_board);
	b.switch_trait();

	Node stand_pat_node(&b);

	int stand_pat_value = stand_pat_node.minimal_quiescence(eval, depth, search_alpha, search_beta, alpha, beta, network);

	return stand_pat_value;
}

// Quiescence minimale (sans stockage des noeuds)
int Node::minimal_quiescence(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha, int beta, Network* network) {
	// REVIEW *** ça reste quand-même très basique (simplement l'évaluation est jugée)

	// Initialisation du noeud
	init_node();

	// Couleur du joueur
	int color = _board->get_color();

	// Evalue la position
	evaluate_position(eval, false, network, true);

	// Stand pat
	int stand_pat = _deep_evaluation._value * color;

	// Si on est en échec (pour ne pas terminer les variantes sur un échec)
	// REVIEW *** est-ce que c'est de trop, si on veut simplement regarder rapidement?
	bool in_check = _board->in_check();

	if (depth <= 0 && !in_check) {
		return stand_pat;
	}

	// Beta cut-off
	if (stand_pat >= beta) {
		return beta;
	}

	// Alpha cut-off
	if (stand_pat > alpha) {
		alpha = stand_pat;
	}

	// Regarde toutes les captures
	for (int i = 0; i < _board->_got_moves; i++) {

		// Coup
		const Move move = _board->_moves[i];

		// Si c'est une capture/échec/promotion, on explore
		if (move.is_capture() || move.is_promotion() || move.is_checkmate()) {

			// Test du delta pruning
			//constexpr int delta = 250;

			//constexpr int piece_values[6] = { 100, 300, 300, 500, 900, 10000 }; // P, N, B, R, Q, K
			//constexpr int promotion_value = 1000;

			//// Estimation rapide de ce que le coup peut apporter
			//const int best_estimation = move.is_promotion() ? promotion_value : move.is_capture() ? piece_values[(_board->_array[move.end_col][move.end_col] - 1) % 6] : 0;

			//if (!move.is_checkmate() && !in_check && stand_pat + best_estimation + delta < alpha) {
			//	// On ne regarde pas ce coup
			//	continue;
			//}

			// Prend une place dans le buffer
			Board new_board(*_board);
			new_board.make_move(move, false, true);
			Node child(&new_board);

			// Appel récursif sur le fils
			int score = -child.minimal_quiescence(eval, depth - 1, search_alpha, search_beta, -beta, -alpha, network);

			// Beta cut-off
			if (score >= beta) {
				return beta;
			}

			// Mise à jour de alpha si l'éval statique est plus grande
			if (score > alpha) {
				alpha = score;
			}
		}
	}

	return alpha;
}

// Constructeur par défaut : n'alloue rien, init() est obligatoire
NodeBuffer::NodeBuffer() {
	_nodes = nullptr;
	_length = 0;
}

// Constructeur taille (octets) : alloue immédiatement
NodeBuffer::NodeBuffer(const size_t size_bytes) {
	init(static_cast<int>(size_bytes / sizeof(Node)), false);
}

// Initialize l'allocation de n plateaux
void NodeBuffer::init(const int length, bool display) {
	if (_init) {
		if (display)
			cout << "node buffer already initialized" << endl;
		return;
	}

	if (display)
		cout << "\ninitializing node buffer..." << endl;

	_length = length;
	_nodes = new Node[_length];

	// Chaque noeud connaît son index ; free-list = tous les indices libres
	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--) {
		_nodes[i]._buffer_index = i;
		_free_indices.push_back(i);
	}

	_init = true;

	if (display) {
		cout << "node buffer initialized" << endl;
		cout << "node size: " << int_to_round_string(sizeof(Node)) << "b" << endl;
		cout << "length: " << int_to_round_string(_length) << endl;
		cout << "approximate buffer size: " << long_int_to_round_string((long long int)_length * sizeof(Node)) << "b\n\n";
	}
}

// Dépile un index libre — O(1). Pile vide => -1 (buffer plein)
int NodeBuffer::get_first_free_index() {
	if (_free_indices.empty())
		return -1;
	const int index = _free_indices.back();
	_free_indices.pop_back();
	return index;
}

// Fonction qui désalloue toute la mémoire
void NodeBuffer::remove() {
	g_buffers_full_logged = false;
	delete[] _nodes;
	_nodes = nullptr;
	_init = false;
	_length = 0;
	_iterator = -1;
	_free_indices.clear();
}

// Reset global du buffer : reconstruit uniquement la pile d'indices libres.
// #12: NE PAS rebalayer _length avec reset(false) (chaque appel clearait un
// robin_map => coût O(capacité)). Sans appelant depuis le fix #12.
bool NodeBuffer::reset() {
	g_buffers_full_logged = false;
	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--)
		_free_indices.push_back(i);

	return true;
}

// Fonction qui renvoie le premier noeud disponible dans le buffer
Node* NodeBuffer::get_first_free_node() {
	const int index = get_first_free_index();
	if (index == -1)
		return nullptr;

	Node* node = &_nodes[index];
	node->_is_active = true;
	return node;
}

// DEBUG *** fonction qui affiche l'état du buffer (combien de plateaux sont utilisés)
void NodeBuffer::display_buffer_state() const {
	int used_boards = 0;
	for (int i = 0; i < _length; i++) {
		if (_nodes[i]._is_active)
			used_boards++;
	}
	cout << "Node buffer state: " << used_boards << " / " << _length << " boards used (" << (used_boards * 100.0 / _length) << "%)" << endl;
}

// Buffer pour l'algo de Monte-Carlo
NodeBuffer monte_node_buffer;

// Log « buffer plein » une seule fois par session de saturation ;
// remis à false dès qu'un reset/remove libère de la place.
bool g_buffers_full_logged = false;
bool g_tt_main_search = false;
bool g_tt_node_dag = false; // #11 Plan B — voir exploration.h
robin_map<uint64_t, Node*> node_map; // #11 Plan B — voir exploration.h
