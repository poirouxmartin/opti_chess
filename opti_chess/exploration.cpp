#include "exploration.h"
#include "useful_functions.h"
#include "zobrist.h"

// Constructeur avec un plateau, un indice et un coup
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

	_children[move] = child;
	//_nodes += child->_nodes;
}

// Fonction qui renvoie le nombre de fils
[[nodiscard]] size_t Node::children_count() const {
	return _children.size();
}

// Fonction qui renvoie le premier coup qui n'a pas encore été ajouté
[[nodiscard]] Move Node::get_first_unexplored_move(bool fully_explored) {
	for (int i = 0; i < _board->_got_moves; i++) {
		Move move = _board->_moves[i];
		if (!_children.contains(move) || (fully_explored && !_children[move]->_fully_explored)) {
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

// Nouveau GrogrosZero
void Node::grogros_zero(Buffer* buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int iterations, int quiescence_depth, Network* network) {
	// TODO:
	// On peut rajouter la profondeur
	// Garder le temps de calcul

	// BUG: rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1
	// quiescence là dessus, après il regarde à donf Tb1 après Dxh7... ??

	//cout << "toto" << endl;

	// Ne devrait pas arriver
	// FIXME!! ça arrive en fait...
	if (iterations <= 0) {
		cout << "iterations <= 0 in grogros_zero" << endl;
		return;
	}

	// Temps de calcul
	const clock_t begin_monte_time = clock();

	// INITIALISATION
	if (!_initialized) {
		// FIXME *** faut-il return ici?

		quiescence(buffer, eval, quiescence_depth, alpha, beta, -INT32_MAX, INT32_MAX, network);
		//_iterations++;
		//_time_spent += clock() - begin_monte_time;

		//return;
	}

	// Si la partie est finie, on ne fait rien
	if (_is_terminal) {
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// Vérifie que le buffer n'est pas plein (TODO) (et est bien initialisé aussi)

	// FIXME
	if (_board->_got_moves <= 0) {
		cout << "no moves in grogros_zero" << endl;
		return;
	}

	while (iterations > 0) {

		// EXPLORATION D'UN NOUVEAU COUP
		if (get_fully_explored_children_count() < _board->_got_moves) {
			explore_new_move(buffer, eval, alpha, beta, gamma, quiescence_depth, network);
		}

		// EXPLORATION D'UN COUP DÉJÀ EXPLORÉ
		else {
			explore_random_child(buffer, eval, alpha, beta, gamma, quiescence_depth, network);
		}

		iterations--;
	}
	
	if (_nodes <= 0) {
		cout << "negative nodes in grogros zero???" << endl;
	}

	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	return;
}

// Fonction qui explore un nouveau coup
void Node::explore_new_move(Buffer* buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network) {

	// On prend le premier coup non exploré
	const Move move = get_first_unexplored_move(true);

	// Noeud fils
	Node *child = nullptr;

	// Si on a déjà exploré ce coup, mais pas complètement
	bool already_explored = _children.contains(move);

	if (already_explored) {
		child = _children[move];

		if (_nodes <= child->_nodes) {
			cout << "child nodes >= nodes???" << endl;
		}

		_nodes -= child->_nodes; // On enlève le nombre de noeuds de ce fils

		//cout << "move already explored, but considered as a new move?" << endl;
	}
	else {
		// FIXME *** fonction pour ça !!

		// Prend une place dans le buffer
		const int buffer_index = buffer->get_first_free_index();

		// FIXME: faut peut-être check ça autre part...
		if (buffer_index == -1) {
			cout << "Buffer is full" << endl;
			return;
		}

		Board* new_board = &buffer->_heap_boards[buffer_index];
		new_board->copy_data(*_board, false, true);
		new_board->_is_active = true;
		new_board->make_move(move, false, false, true);

		// Cela transpose t-il dans une autre branche?
		new_board->get_zobrist_key();

		// TEST: 8/2k5/3p4/p2P1p2/P2P1P2/8/3K4/8 w - - 10 6
		// Quand on update un des noeuds, il faudra potentiellement backpropagate dans toutes les branches parentes...
		// Gros problème de noeuds négatifs

		bool transpositions = false;


		// Si la position est déjà dans la table de transposition
		if (transposition_table.contains(new_board->_zobrist_key) && transpositions) {
			cout << "transposition: "<< new_board->to_fen() << endl;

			child = transposition_table._hash_table[new_board->_zobrist_key]._node;
			
			cout << "transposition nodes: " << child->_nodes << endl;
			cout << "parent nodes: " << _nodes << endl;

			//_nodes += child->_nodes;

			//cout << "new transposition nodes: " << _nodes << endl;
		}

		// Sinon, on crée un nouveau noeud
		else {
			// Création du noeud fils
			child = new Node(new_board);

			// Ajoute la position dans la table
			
			if (transpositions) {
				transposition_table._hash_table[new_board->_zobrist_key] = ZobristEntry(child);
			}

			//cout << "normal" << endl;
		}
	}

	// Evalue le plateau, en regardant si la partie est finie
	//child->_board->evaluate(eval, false, nullptr, true);
	//child->_deep_evaluation._value = child->_board->quiescence(eval, -INT32_MAX, INT32_MAX, 4, true) * child->_board->get_color();
	//child->_nodes = 1;

	if (child == nullptr) {
		cout << "null child and shouldn't be (explore_new_move)" << endl;
	}

	if (!child->_fully_explored) {
		child->quiescence(buffer, eval, quiescence_depth, alpha, beta, -INT32_MAX, INT32_MAX, network);
		child->_fully_explored = true;
	}

	// rnb1kbnr/ppp1pppp/2q5/1B6/8/2N5/PPPP1PPP/R1BQK1NR b KQkq - 3 4
	// rnb1kbnr/ppp1pppp/2q5/8/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4 : ici Fb5 -> +114 au lieu de +895

		// Augmente le nombre de noeuds
	//_nodes++;
	//cout << "main nodes:" << _nodes << ", child nodes:" << child->_nodes << endl;
	_nodes += child->_nodes;

	if (_nodes <= 0) {
		cout << "negative nodes in explore_new_move???" << endl;
	}

	if (child->_nodes <= 0) {
		cout << "negative nodes in explore_new_move child???" << endl;
	}

	//cout << "total: " << _nodes << endl;
	child->_iterations = 1;
	child->_chosen_iterations = 1;
	_iterations++;

	// Ajoute le fils
	if (!already_explored) {
		//cout << "new child" << endl;
		add_child(child, move);
	}
	else {
		//cout << "ALREADY EXPLORED! should we add nodes here??" << endl;
		// FIXME: this should be reviewed
	}

	// Tous les coups ont-ils déjà été explorés?
	// FIXME: faut-il seulement évaluer si tous les coups ont été entièrement explorés?
	//bool all_moves_explored = (get_fully_explored_children_count() + !already_explored) == _board->_got_moves;
	bool all_moves_explored = (get_fully_explored_children_count()) == _board->_got_moves;
	//bool all_moves_explored = (children_count() + !already_explored) == _board->_got_moves;

	// Met à jour l'évaluation du plateau


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
		_deep_evaluation = _children[best_move]->_deep_evaluation;
	}



	//if (all_moves_explored) {
	//	_is_stand_pat_eval = false;

	//	//Evaluation best_eval = Evaluation();

	//	//for (auto const& [move_2, child_2] : _children) {
	//	//	if (_board->_player ? child_2->_deep_evaluation > best_eval : child_2->_deep_evaluation < best_eval) {
	//	//		best_eval = child_2->_deep_evaluation;
	//	//	}
	//	//}

	//	//// Il faut aussi regarder le coup qu'on vient d'explorer
	//	////if (_board->_player ? child->_deep_evaluation > best_eval : child->_deep_evaluation < best_eval) {
	//	////	best_eval = child->_deep_evaluation;
	//	////}

	//	//_deep_evaluation = best_eval;

	//	_deep_evaluation = _children[get_best_score_move(alpha, beta)]->_deep_evaluation;
	//}

	//// Tous les coups ne sont pas encore explorés, donc on met à jour l'évaluation du plateau avec le coup exploré
	//else {
	//	if (_board->_player) {
	//		if (child->_deep_evaluation._value > _deep_evaluation._value) {
	//			_deep_evaluation = child->_deep_evaluation;
	//			_is_stand_pat_eval = false;
	//		}
	//	}
	//	else {
	//		if (child->_deep_evaluation._value < _deep_evaluation._value) {
	//			_deep_evaluation = child->_deep_evaluation;
	//			_is_stand_pat_eval = false;
	//		}
	//	}
	//}

	// FIXME: si on a regardé tous les fils, et qu'aucun des coups n'améliore l'évaluation, on fait quoi?
	// - Option 1: on garde l'évaluation sans aucun coup
	// - Option 2: on garde l'évaluation du meilleur coup
	// Est-ce vraiment grave? peut-être pas car si on continue une profondeur plus loin, explore_random_child() va prendre le meilleur coup
}

// Fonction qui explore dans un plateau fils pseudo-aléatoire
void Node::explore_random_child(Buffer* buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network) {

	// Prend un fils aléatoire
	const Move move = pick_random_child(alpha, beta, gamma);
	Node *child = _children[move];

	if (child->_nodes >= _nodes) {
		cout << "child nodes >= nodes in random exploration???" << endl;
	}

	// Nombre de noeuds du fils
	const int initial_child_nodes = child->_nodes;

	// On explore ce fils
	child->grogros_zero(buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network); // L'évaluation du fils est mise à jour ici

	// Met à jour l'évaluation du plateau
	//Evaluation best_eval = Evaluation();

	//for (auto const& [move_2, child_2] : _children) {
	//	if (_board->_player ? child_2->_deep_evaluation > best_eval : child_2->_deep_evaluation < best_eval) {
	//		best_eval = child_2->_deep_evaluation;
	//	}
	//}

	//_deep_evaluation = best_eval;

	_deep_evaluation = _children[get_best_score_move(alpha, beta)]->_deep_evaluation;

	// Augmente le nombre de noeuds
	_nodes += child->_nodes - initial_child_nodes;

	if (_nodes <= 0) {
		cout << "negative nodes in explore_random_child???" << endl;
	}

	if (child->_nodes <= 0) {
		cout << "negative nodes in explore_random_child child???" << endl;
	}

	// Augmente le nombre d'itérations
	_iterations++;
}

// Fonction qui renvoie le fils le plus exploré
[[nodiscard]] Move Node::get_most_explored_child_move(bool decide_by_eval) {
	int max = -1;

	// Tri simple, on ne départage pas les égalités
	if (!decide_by_eval) {

		Move best_move = Move();

		for (auto const& [move, child] : _children) {
			if (child->_chosen_iterations > max) {
				max = child->_chosen_iterations;
				best_move = move;
			}
		}

		return best_move;
	}

	// Avec un départage par égalité
	else {
		vector<Move> max_iterations_moves;
		int color = _board->get_color();

		for (auto const& [move, child] : _children) {
			if (child->_chosen_iterations == max) {
				max_iterations_moves.push_back(move);
			}
			else if (child->_chosen_iterations > max) {
				max = child->_chosen_iterations;
				max_iterations_moves.clear();
				max_iterations_moves.push_back(move);
			}
		}

		// En cas d'égalité, on trie par évaluation
		long long int max_eval = -LLONG_MAX;
		auto best_move = Move();

		for (auto const& move : max_iterations_moves) {
			Node const* child = _children[move];

			if (child->_deep_evaluation._value * color > max_eval) {
				max_eval = child->_deep_evaluation._value * color;
				best_move = move;
			}
		}

		return best_move;
	}
}

// Reset le noeud et ses enfants, et les supprime tous
void Node::reset() {
	_latest_first_move_explored = -1;
	_nodes = 0;
	_iterations = 0;
	_chosen_iterations = 0;
	_board->reset_board();
	_initialized = false;
	_is_terminal = false;
	_can_explore = true;
	_time_spent = 0;
	_fully_explored = false;
	_static_evaluation.reset();
	_deep_evaluation.reset();
	_quiescence_depth = 0;

	for (auto const& [_, child] : _children) {
		child->reset();
		delete child;
	}

	_children.clear();
}

// Fonction qui renvoie les variantes d'exploration
string Node::get_exploration_variants(const double alpha, const double beta, bool main, bool quiescence) {

	// Si on est en fin de variante
	if (_board->_game_over_value) {
		return "";
	}

	string variants;

	// S'il y a des coups explorés
	if (children_count() > 0) {

		// Si on est dans le noeud principal, on affiche toutes les variantes
		if (main) {
			// TODO: améliorer ce tri...

			// Trie les enfants par nombre d'itérations par l'algo de GrogrosZero
			vector<pair<int, Move>> children_iterations;

			for (auto const& [move, child] : _children) {
				children_iterations.emplace_back(-child->_chosen_iterations, move); // On met un moins pour trier dans l'ordre décroissant
			}

			std::ranges::sort(children_iterations.begin(), children_iterations.end());

			// TODO *** trier secondairement par score de coup

			for (auto const& [neg_child_iterations, move] : children_iterations) {
				Node* child = _children[move];
				const int child_iterations = -neg_child_iterations;
				const int child_chosen_iterations = child->_chosen_iterations;
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
				child_variants += " ";
				child_variants += child->get_exploration_variants(alpha, beta, false, new_quiescence || quiescence);

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
			//const Move best_move = get_most_explored_child_move();
			const Move best_move = get_best_score_move(alpha, beta, true);

			// Standpat
			if (best_move.is_null_move()) {
				variants += "...";
			}
			else {
				const bool new_quiescence = !quiescence && _children[best_move]->_iterations == 0;

				//variants += (new_quiescence ? "(" + _board->evaluation_to_string(_deep_evaluation._value) + ") (" : "") + (_board->_player ? to_string(_board->_moves_count) + ". " : "") + _board->move_label(best_move, true) + " " + _children[best_move]->get_exploration_variants(alpha, beta, false, new_quiescence || quiescence) + (new_quiescence ? ")" : "");
				variants += (new_quiescence ? "(" : "") + (_board->_player ? to_string(_board->_moves_count) + ". " : "") + _board->move_label(best_move, true) + " " + _children[best_move]->get_exploration_variants(alpha, beta, false, new_quiescence || quiescence) + (new_quiescence ? ")" : "");
			}
		}
		
	}

	// S'il n'y a pas de coups explorés
	else {

		// On affiche l'évaluation du plateau en fin de variante
		//variants = "(" + _board->evaluation_to_string(_deep_evaluation._value) + ")";
		variants = "";
	}
	
	return variants;
}

// Fonction qui renvoie la profondeur de la variante principale
[[nodiscard]] int Node::get_main_depth(const double alpha, const double beta) {
	if (children_count() > 0) {
		//Move main_move = get_most_explored_child_move();
		Move main_move = get_best_score_move(alpha, beta, true);

		if (main_move.is_null_move()) {
			//cout << "most_explored move is null???" << endl;
			return 0;
		}

		Node* main_child = _children[main_move];

		return main_child->get_main_depth(alpha, beta) + 1;
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
[[nodiscard]] Node* Node::get_most_explored_child(bool decide_by_eval) {
	Move most_explored_move = get_most_explored_child_move(decide_by_eval);

	if (most_explored_move.is_null_move()) {
		return nullptr;
	}

	return _children[most_explored_move];
}

// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
[[nodiscard]] int Node::get_avg_nps() const {
	return _time_spent == 0 ? 0 : ((double)_nodes / (double)_time_spent * CLOCKS_PER_SEC);
}

// Fonction qui renvoie le nombre d'itérations par seconde
[[nodiscard]] int Node::get_ips() const {
	return _time_spent == 0 ? 0 : ((double)_iterations / (double)_time_spent * CLOCKS_PER_SEC);
}

// Quiescence search intégré à l'exploration
int Node::quiescence(Buffer* buffer, Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha, int beta, Network* network, bool evaluate_threats, int beta_margin) {
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

	//cout << "depth: " << depth << endl;
	//rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3 : ????

	// 4r3/4b1p1/p2B2k1/7p/1p4p1/1P6/P1P1RPP1/5K2 b - - 3 40 : pas de quiescence???

	// r1bqk2r/ppp2ppp/1b6/1P1nP3/2B5/5P2/P5PP/RNBQK1NR b KQkq - 0 10

	//FIX!! r3r3/P2k2pp/1Bb5/6N1/3P1n2/1Q1q4/PP3PPP/1KR5 w - - 3 27 : quiescence qui ne va pas jusqu'au bout? (peut-être car c'est moins bon que le standpat?)
	// 3k4/3p1p2/5P2/8/3qP2P/8/PrPQ1R2/R3K1r1 w Q - 4 32 : pareil, la quiescence?
	// 3k4/3p4/8/8/3q4/8/3Q1R2/4K1r1 w - - 4 32 : test

	// 3k4/3p1p2/5P2/8/3qP2P/8/PrPQ4/R4K2 b - - 0 33 : 0 quiescence ici?

	// On a au moins évalué le plateau du noeud
	if (_nodes > 0) {
		//cout << "quiescence nodes from already explored:" << _nodes << endl;
	}
	else {
		_nodes = 1;
	}

	// Temps de calcul
	const clock_t begin_monte_time = clock();

	// Initialisation du noeud
	init_node();

	//cout << "quiescence: " << _board->_moves_flags_assigned << endl;

	// Couleur du joueur
	int color = _board->get_color();

	// Evalue la position
	if (!_static_evaluation._evaluated) {
		//string test = _board->to_fen();
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
	//int stand_pat = (custom_stand_pat ? stand_pat_value : _deep_evaluation._value) * color;
	int stand_pat = _deep_evaluation._value * color;
	
	// FIXME *** we can't have the actual usable score of this standpat, since it would require to know the evaluations of all the children
	// Instead we use a previsional value, assuming that the standpat is the best move
	//int stand_pat = get_previsonal_node_score(search_alpha, search_beta, color);
	//cout << "evaluation: " << _deep_evaluation._value << ", score: " << _deep_evaluation._avg_score << ", standpat: " << stand_pat << endl;

	// Si on est en échec (pour ne pas terminer les variantes sur un échec)
	const bool in_check = _board->in_check();

	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : le check extension fonctionne pas?

	// Emergency cutoff: depth - 4
	if (depth <= -4) {
		_time_spent += clock() - begin_monte_time;
		cout << "emergency cutoff: " << _board->to_fen() << ", in_check: " << in_check << endl;
		return stand_pat;
	}

	// Profondeur nulle, on renvoie le standpat
	if (depth <= 0 && !in_check) {
		_time_spent += clock() - begin_monte_time;

		if (depth < 0) {
			//cout << "quiescence depth < 0: " << depth << ", " << _board->to_fen() << ", in_check: " << in_check << endl;
		}
		//cout << "stand pat" << endl;
		return stand_pat;
	}

	// 1r1q1r1k/2n4p/p2p1p1Q/2ppn3/7N/4R3/1PP1N1PP/5R1K w - - 1 24 : quiescence buggée après Txe5??????????
	// 1nb2rk1/r4ppp/p4q2/1Bp1bN2/8/2N2Q2/PPP2PPP/3RK2R w K - 1 17 : après Fxc3, ça va pas au bout des variantes...
	// 2rqr1k1/pNbnnpp1/2p1p1p1/P2pP3/Q2P4/B1P4P/4BPP1/RR4K1 b - - 6 22 : menace la dame en d8

	// Marge, qui dépend de la menace adverse (jugée grâce à la valeur de la quiescence sur le tour de l'adversaire)
	if (evaluate_threats && !in_check) {
		// Fait une courte quiescence pour évaluer la menace
		int new_stand_pat = -evaluate_quiescence_threat(eval, 2, search_alpha, search_beta, -INT32_MAX, INT32_MAX, network);
		//int new_stand_pat = -evaluate_quiescence_threat(eval, 4, search_alpha, search_beta, -INT32_MAX, INT32_MAX, network);

		//cout << "new_stand_pat: " << new_stand_pat << ", test: " << test << endl;

		// FIXME *** il faut aussi prendre en compte que le trait ayant changé de camp, il y a une petite variation d'évaluation due à cela...
		// Marge de 100 en moins
		new_stand_pat += 100;

		beta_margin = max(0, stand_pat - new_stand_pat);

		//cout << "Pos: " << _board->to_fen() << ", stand_pat: " << stand_pat << ", new_stand_pat: " << new_stand_pat << ", beta_margin: " << beta_margin << endl;
	}

	// TODO *** ne propager la margin seulement sur une profondeur plus petite que la profondeur utilisée pour la threat evaluation?
	// Experimental
	//if (!evaluate_threats) {
	//	// Réduit la marge
	//	beta_margin *= 0.5;
	//	//cout << "test margin: " << beta_margin << endl;
	//}

	// Si on est en échec, il ne faut jamais faire de beta cutoff
	double total_beta = in_check ? INT_MAX : (double)beta + beta_margin;

	// FIXME *** normal qu'on envoie la beta margin sur toute la profondeur?

	if (stand_pat >= total_beta) {
		_time_spent += clock() - begin_monte_time;
		//cout << "beta cutoff1: " << stand_pat << " >= " << beta << " + " << beta_margin << endl;
		return beta;
	}

	// Test du standpat pruning (futility pruning)
	//constexpr int standpat_pruning_threshold = 300;

	//if (!in_check && stand_pat + standpat_pruning_threshold <= alpha)
	//	return stand_pat; // Trop pourri pour améliorer quoi que ce soit

	// Mise à jour de alpha si l'éval statique est plus grande
	if (stand_pat > alpha && !in_check) {
	//if (stand_pat > alpha) {
		alpha = stand_pat;
	}

	int move_index = 0;

	// Regarde toutes les captures
	// FIXME: faudra regarder que les coups non explorés? (ou gérer avec la profondeur...)
	// TODO: get_next_capture()?
	for (int i = 0; i < _board->_got_moves; i++) {

		// Coup
		const Move move = _board->_moves[i];

		//cout << "move: " << _board->move_label(move) << ", flags: " << move.is_capture << endl;

		// Ce coup a-t-il déjà été exploré?
		//int child_index = get_child_index(move);
		//bool already_explored = child_index != -1;

		bool already_explored = _children.contains(move);

		//if (already_explored) {
		//	cout << "move already explored" << endl;
		//	// FIXME: IL FAUT GERER CE CAS-LA!!!
		//	continue;
		//}
		// 
		//8/3P4/3r3p/P4p2/8/P2P1k1P/7K/1R6 b - - 4 49

		// *** FAUT-IL EXPLORER CE COUP? ***

		// Si on est en échec, on explore tous les coups
		//const bool should_explore = in_check || move.is_capture() || move.is_promotion() || move.is_checkmate();
		const bool should_explore = in_check || move.is_capture() || move.is_promotion() || move.is_checkmate() || move.is_check();

		// 1knr4/8/Qp3R1p/1N1r4/1P4p1/5P2/6PP/R6K b - - 0 36 : il regarde pas Td1+?

		//cout << "move: " << _board->move_label(move) << ", should_explore: " << should_explore << ", already_explored: " << already_explored << endl;

		/*if (should_explore)
			cout << "depth: " << depth << " | move: " << _board->move_label(move) << " | check_extension: " << check_extension << endl;*/

		// *** EXPLORATION ***
		// Si c'est une capture/échec/promotion, on explore
		if (should_explore) {

			constexpr int check_extension = 0; // On prolonge la profondeur de 1 si on est en échec

			//cout << endl << "position: " << _board->to_fen() << endl;
			//cout << "initial depth: " << depth << ", move_index: " << move_index << ", move: " << _board->move_label(move) << endl;

			int new_depth = depth;

			new_depth += move.is_check() ? check_extension : 0;

			// Réduction de la profondeur pour les coups moins prometteurs
			new_depth -= in_check ? 0 : move_index;

			// FIXME : ne pas réduire si on est à depth < 2?

			//cout << "reduction: " << (in_check ? 0 : move_index) << endl;

			//cout << "depth after reduction: " << depth << endl;

			if (new_depth <= 0 && !in_check) {
				//cout << "depth <= 0, skipping move" << endl;
				continue; // On ne regarde plus les coups si on est trop profond
			}

			move_index++;

			if (!move.is_checkmate() && !in_check) {
				// Test du delta pruning
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
					//cout << "delta pruning: " << _board->move_label(move) << ", stand_pat: " << stand_pat << ", best_estimation: " << best_estimation << ", delta: " << delta << ", alpha: " << alpha << endl;
					continue; // On ne regarde pas ce coup
				}
			}

			Node *child = nullptr;

			// Si on a déjà exploré ce coup, mais pas complètement
			if (already_explored) {
				//cout << "move already explored" << endl;

				child = _children[move];
			}

			else {
				// Prend une place dans le buffer
				const int buffer_index = buffer->get_first_free_index();

				// FIXME: faut peut-être check ça autre part...
				if (buffer_index == -1) {
					cout << "Buffer is full" << endl;
					_time_spent += clock() - begin_monte_time;

					return alpha; // faut renvoyer quoi?
				}

				Board* new_board = &buffer->_heap_boards[buffer_index];
				new_board->copy_data(*_board, false, true);
				new_board->_is_active = true;
				new_board->make_move(move, false, false, true);

				// Création du noeud fils
				child = new Node(new_board);
				//cout << get_child_index(move) << endl;
				add_child(child, move);
			}

			// Appel récursif sur le fils
			// FIXME *** à backpropagate seulement sur une profondeur plus petite que la profondeur utilisée pour la threat evaluation?
			int score = - child->quiescence(buffer, eval, new_depth - 1, search_alpha, search_beta, -beta, -alpha, network, false, beta_margin);
			//int score = - child->quiescence(buffer, eval, depth - 1, search_alpha, search_beta, -beta, -alpha, network, false, 0);
			_nodes += child->_nodes;

			// Mise à jour de l'évaluation du plateau
			// FIXME: ici, s'il y'a un seul coup, il faut mettre à jour l'évaluation du plateau même si l'évaluation fils est moins bonne
			// Ou alors: si tous les coups ont été explorés, on met à jour l'évaluation du plateau avec le meilleur coup
			bool all_moves_explored = children_count() == _board->_got_moves;

			// 1r1q1r1k/2n4p/p2p1p1Q/2ppR3/7N/8/1PP1N1PP/5R1K b - - 0 24 : quiescence ici

			// Tous les coups ont été explorés, donc on met à jour l'évaluation du plateau avec le meilleur coup
			Move best_move = get_best_score_move(search_alpha, search_beta, !all_moves_explored, new_depth - 1);

			// Standpat = le meilleur
			if (best_move.is_null_move()) {
				//_is_stand_pat_eval = true;
			}
			else {
				//_is_stand_pat_eval = false;
				_deep_evaluation = _children[best_move]->_deep_evaluation;
				if (_children[best_move]->_quiescence_depth != new_depth - 1) {
					cout << "expected depth: " << new_depth - 1 << ", actual depth: " << _children[best_move]->_quiescence_depth << endl;
				}
			}

			// Beta cut-off
			if (score >= beta) {
			//if (score >= total_beta) {
				_time_spent += clock() - begin_monte_time;
				//cout << "move: " << _board->move_label(move) << " | beta cut-off2: " << score << " >= " << beta << " + " << beta_margin << endl;
				return beta;
			}

			// Mise à jour de alpha si l'éval statique est plus grande
			//if (stand_pat > alpha && !in_check) {
			if (score > alpha) {
				alpha = score;
			}
		}
	}

	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	//cout << "alpha: " << alpha << endl;
	//_deep_evaluation._value = alpha * color;

	return alpha;
}

// Fonction qui renvoie le nombre de noeuds fils complètement explorés
[[nodiscard]] int Node::get_fully_explored_children_count() const {
	int count = 0;

	for (auto const& [_, child] : _children) {
		if (child->_fully_explored) {
			count++;
		}
	}

	return count;
}

// Fonction qui renvoie la somme des noeuds des fils
int Node::count_children_nodes() const {
	int sum = 0;

	for (auto& [move, child] : _children) {
		sum += child->get_total_nodes();
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
void Node::evaluate_position(Evaluator* eval, bool display, Network * network, bool game_over_check) {
	_board->evaluate(eval, display, network, game_over_check);
	_deep_evaluation._value = _board->_evaluation;
	_deep_evaluation._uncertainty = _board->_uncertainty;
	_deep_evaluation._winnable_white = _board->_winnable_white;
	_deep_evaluation._winnable_black = _board->_winnable_black;
	_deep_evaluation._wdl = _board->_wdl;
	_deep_evaluation._avg_score = _board->get_average_score();
	_deep_evaluation._evaluated = true;

	_static_evaluation = _deep_evaluation;
}

// Fonction qui renvoie un noeud fils pseudo-aléatoire (en fonction des évaluations et du nombre de noeuds)
Move Node::pick_random_child(const double alpha, const double beta, const double gamma) {
	// TESTS
	// 8/8/8/1r5p/2p4k/2Kb4/8/8 b - - 1 69 : tout égal quand tout gagne...
	// r2qr1k1/3bbp1p/p2pn1p1/3QP3/3P4/3B1N2/1P1B1PPP/R3R1K1 w - - 1 24 : pareil
	// 3b2rk/3P2pp/8/p7/8/2Q1p3/PP1p1pPP/3RqR1K b - - 1 36 : il faut pas 100% de reflexion sur un coup, quand tous les coups gagnent
	// 8/8/8/1r5p/2p2k2/2Kb4/8/8 b - - 5 71 : pareil...
	// R3r1k1/1P3p2/3p2p1/5nb1/4r3/2P1p2P/4N3/2BK4 w - - 2 36 : il faut regarder Tc8 si toutes les réponses adverses sont pourries

	// r1r3k1/pp2bppp/3q4/3Pnp2/4nB2/2NB4/PP3PPP/R2QR1K1 w - - 5 16 : ???

	// Meilleur coup global
	Move best_move = Move();
	double best_score = -DBL_MAX;

	// Meilleur coup explorable
	Move move_to_play = Move();
	double explorable_best_score = -DBL_MAX;

	// Scores des coups

	int color = _board->get_color();

	// Meilleure valeur d'évaluation
	int max_eval = -INT_MAX;

	// Meilleure chance de gagner
	double max_avg_score = 0.0;

	for (auto const& [_, child] : _children) {
		if (child->_deep_evaluation._value * color > max_eval) {
			max_eval = child->_deep_evaluation._value * color;
		}

		if (_board->_player ? child->_deep_evaluation._avg_score > max_avg_score : 1 - child->_deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? child->_deep_evaluation._avg_score : 1 - child->_deep_evaluation._avg_score;
		}
	}

	robin_map<Move, double> move_scores = get_move_scores(alpha, beta);

	// TEST: boost pour les meilleurs coups, pour qu'il évite de tout regarder à faible profondeur...
	// Trie les coups par score
	vector<pair<Move, double>> sorted_moves(move_scores.begin(), move_scores.end());

	// Tri par valeur décroissante
	sort(sorted_moves.begin(), sorted_moves.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

	// Boost pour les premiers coups: *2, *1.5, *1.25, *1.1
	constexpr double constant_add = 0.000;

	for (int i = 0; i < sorted_moves.size(); i++) {
		double boost = 1.0 + 20.0 / pow(i + 1, 1.5);
		move_scores[sorted_moves[i].first] = boost * move_scores[sorted_moves[i].first] + constant_add;
		//cout << "boosted move score: " << _board->move_label(sorted_moves[i].first) << " : " << move_scores[sorted_moves[i].first] << endl;
	}


	// Regarde chaque coup
	for (auto const& [move, child] : _children) {
	//for (auto const& [move, move_score] : sorted_moves) {
		
		//Node* child = _children[move];

		// Score du coup
		double move_score = move_scores[move];

		// Facteur d'exploration
		int child_iterations = max(child->_chosen_iterations, child->_iterations);

		// FIXME *** gamma devrait changer en fonction de l'incertitude: plus on est incertain, plus on explore large?
		const double new_gamma = gamma / (1.00f - _board->_uncertainty / 2.0f) / (1.00f - _board->_adv / 2.0f);
		//const double new_gamma = gamma;
		//cout << "gamma: " << gamma << ", uncertainty: " << _board->_uncertainty << ", new_gamma: " << new_gamma << endl;

		double exploration_score = child_iterations == 0 ? _iterations * 2 : pow((double)_iterations / (double)child_iterations, new_gamma);
		//const double test = 5.0;
		//double exploration_score = child_iterations == 0 ? _iterations * 2 : pow(pow((double)_iterations, 1 / test) / pow((double)child_iterations, 1 / test), test);

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

			//cout << "move: " << _board->move_label(move) << endl;

			for (auto const& [move2, child2] : child->_children) {
				//cout << "sub-move: " << child->_board->move_label(move2) << " | fully explored: " << child2->_fully_explored << " | eval: " << child2->_deep_evaluation._value << " / " << best_eval._value << endl;

				//if (!child2->_fully_explored) {
				//	continue;
				//}

				if (child->_board->_player ? (child2->_deep_evaluation > best_eval) : (child2->_deep_evaluation < best_eval)) {
					best_eval = child2->_deep_evaluation;
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

	// Meilleur coup global
	_children.at(best_move)->_chosen_iterations++;

	if (move_to_play.is_null_move()) {
		return best_move;
	}

	return move_to_play;
}

// Fonction qui renvoie le score des coup
robin_map<Move, double> Node::get_move_scores(const double alpha, const double beta, const bool consider_standpat, const int qdepth) {

	// Pour le standpat, on l'associe au null move

	// TEST: 8/8/8/1r5p/2p2k2/2Kb4/8/8 b - - 5 71

	int color = _board->get_color();

	// Meilleure valeur d'évaluation
	int max_eval = -INT_MAX;

	// Meilleure chance de gagner
	double max_avg_score = 0.0;

	// Cherche la meilleure eval et le meilleure score parmi tous les coups possibles
	for (auto const& [_, child] : _children) {
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
	for (auto const& [move, child] : _children) {
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
	const double pure_win_chance_adding = 0.0001f; // Bonus si on a des chances de gagner pures
	//const double pure_win_chance_adding = 0.0f; // Bonus si on a des chances de gagner pures

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
	const double win_adding = (player ? eval._wdl.win_chance : eval._wdl.lose_chance) * pure_win_chance_adding;
	//cout << "win chance: " << (player ? eval._wdl.win_chance : eval._wdl.lose_chance) << ", win_adding: " << win_adding << endl;

	//cout << "eval: " << eval_score << ", score: " << score_score << endl;

	// Facteur 3: rajout quasi constant? à tester...
	const double adding = (avg_score == 0.0f) ? 0.0f : avg_score / max_avg_score * add_constant;

	// Score final
	const double score = eval_score * score_score + adding + win_adding;

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
	for (auto const& [_, child] : _children) {
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

	for (auto const& [move, child] : _children) {
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
			new_board.make_move(move, false, false, true);
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