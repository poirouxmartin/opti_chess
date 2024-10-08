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

// Nouveau GrogrosZero
void Node::grogros_zero(Buffer* buffer, Evaluator* eval, float beta, float k_add, int iterations, int quiescence_depth, Network* network) {
	// TODO:
	// On peut rajouter la profondeur
	// Garder le temps de calcul

	// BUG: rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1
	// quiescence là dessus, après il regarde à donf Tb1 après Dxh7... ??

	// Ne devrait pas arriver
	// FIXME!! ça arrive en fait...
	if (iterations <= 0) {
		cout << "iterations <= 0 in grogros_zero" << endl;
		return;
	}

	// Temps de calcul
	const clock_t begin_monte_time = clock();

	// INITIALISATION
	if (_new_node) {
		grogros_quiescence(buffer, eval, quiescence_depth, -INT32_MAX, INT32_MAX, network);
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// Si la partie est finie, on ne fait rien
	if (_board->_game_over_value) {
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// Vérifie que le buffer n'est pas plein (TODO) (et est bien initialisé aussi)
	

	while (iterations > 0) {

		// EXPLORATION D'UN NOUVEAU COUP
		if (get_fully_explored_children_count() < _board->_got_moves) {
			explore_new_move(buffer, eval, quiescence_depth, network);
		}

		// EXPLORATION D'UN COUP DÉJÀ EXPLORÉ
		else {
			explore_random_child(buffer, eval, beta, k_add, quiescence_depth, network);
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
void Node::explore_new_move(Buffer* buffer, Evaluator* eval, int quiescence_depth, Network* network) {

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

		// Si la position est déjà dans la table de transposition
		if (transposition_table.contains(new_board->_zobrist_key)) {
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
			bool transpositions = false;
			
			if (transpositions) {
				transposition_table._hash_table[new_board->_zobrist_key] = ZobristEntry(child);
			}

			//cout << "normal" << endl;
		}
	}

	// Evalue le plateau, en regardant si la partie est finie
	//child->_board->evaluate(eval, false, nullptr, true);
	//child->_board->_evaluation = child->_board->quiescence(eval, -INT32_MAX, INT32_MAX, 4, true) * child->_board->get_color();
	//child->_nodes = 1;

	if (child == nullptr) {
		cout << "null child and shouldn't be (explore_new_move)" << endl;
	}

	if (!child->_fully_explored) {
		child->grogros_quiescence(buffer, eval, quiescence_depth, -INT32_MAX, INT32_MAX, network);
		//child->_board->evaluate(eval, false, nullptr, true);
		//child->_nodes = 1;

		child->_fully_explored = true;
	}

	// rnb1kbnr/ppp1pppp/2q5/1B6/8/2N5/PPPP1PPP/R1BQK1NR b KQkq - 3 4
	// rnb1kbnr/ppp1pppp/2q5/8/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4 : ici Fb5 -> +114 au lieu de +895

	// Tous les coups ont-ils déjà été explorés?
	// FIXME: faut-il seulement évaluer si tous les coups ont été entièrement explorés?
	//bool all_moves_explored = get_fully_explored_children_count() == _board->_got_moves;
	bool all_moves_explored = children_count() == _board->_got_moves;

	// Met à jour l'évaluation du plateau
	if (!all_moves_explored) {
		_board->_evaluation = _board->_player * max(_board->_evaluation, child->_board->_evaluation) + !_board->_player * min(_board->_evaluation, child->_board->_evaluation);
	}

	// Tous les coups ont été explorés, donc on met à jour l'évaluation du plateau avec le meilleur coup
	else {
		int color = _board->get_color();
		int best_eval = -INT_MAX;

		for (auto const& [_, child_2] : _children) {
			if (child_2->_board->_evaluation * color > best_eval) {
				best_eval = child_2->_board->_evaluation * color;
			}
		}

		if (best_eval == -INT_MAX) {
			cout << "new -max eval" << endl;
		}

		_board->_evaluation = best_eval * color;
	}

	// FIXME optimiser ces cas pour n'en calculer qu'un

	// Si tous les coups ont été explorés, on met à jour l'évaluation du plateau avec le meilleur coup
	//if (children_count() == _board->_got_moves) {
	//	int color = _board->get_color();
	//	int best_eval = -INT_MAX;

	//	for (auto& [move, child] : _children) {
	//		if (child->_board->_evaluation * color > best_eval) {
	//			best_eval = child->_board->_evaluation * color;
	//		}
	//	}

	//	_board->_evaluation = best_eval * color;
	//}
	

	// FIXME: si on a regardé tous les fils, et qu'aucun des coups n'améliore l'évaluation, on fait quoi?
	// - Option 1: on garde l'évaluation sans aucun coup
	// - Option 2: on garde l'évaluation du meilleur coup
	// Est-ce vraiment grave? peut-être pas car si on continue une profondeur plus loin, explore_random_child() va prendre le meilleur coup

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
}

// Fonction qui explore dans un plateau fils pseudo-aléatoire
void Node::explore_random_child(Buffer* buffer, Evaluator* eval, float beta, float k_add, int quiescence_depth, Network* network) {

	// Prend un fils aléatoire
	const Move move = pick_random_child(beta, k_add);
	Node *child = _children[move];

	if (child->_nodes >= _nodes) {
		cout << "child nodes >= nodes in random exploration???" << endl;
	}

	// Nombre de noeuds du fils
	const int initial_child_nodes = child->_nodes;

	// On explore ce fils
	child->grogros_zero(buffer, eval, beta, k_add, 1, quiescence_depth, network); // L'évaluation du fils est mise à jour ici

	// Met à jour l'évaluation du plateau
	if (_board->_player) {
		_board->_evaluation = -INT_MAX;

		for (auto const& [_, child_2] : _children) {
			_board->_evaluation = max(_board->_evaluation, child_2->_board->_evaluation);
		}

		if (_board->_evaluation == INT_MAX) {
			cout << "eval min" << endl;
		}
	}
	else {
		_board->_evaluation = INT_MAX;

		for (auto const& [_, child_2] : _children) {
			_board->_evaluation = min(_board->_evaluation, child_2->_board->_evaluation);
		}

		if (_board->_evaluation == INT_MAX) {
			cout << "eval max" << endl;
		}
	}

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

// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variantes, en fonction de la grandeur du nombre correspondant à cet index
Move Node::pick_random_child(const float beta, const float k_add) const {
	// FIXME: ajouter quelque chose comme l'évaluation relative pour choisir les coups plutôt que les évaluations brutes... sinon, dans les évaluations énormes, il regarde un seul coup...

	const int color = _board->get_color();
	auto n_children = static_cast<int>(children_count());

	// *** 1. ***
	// Evaluations des enfants (en fonction de la couleur)
	double l2[100]{};
	map<int, Move> children_moves;
	double max_eval = -DBL_MAX;

	int i = 0;
	for (auto const& [move, child] : _children) {
		l2[i] = color * child->_board->_evaluation;

		if (l2[i] > max_eval) {
			max_eval = l2[i];
		}

		children_moves[i] = move;
		i++;
	}

	//cout << "new: " << endl;
	//print_array(l2, n_children);

	// Plus les évaluations sont élevées, plus on veut regarder large (EXPERIMENTAL)
	
	// Facteur d'élargissement de la distribution
	//double enlargement_factor = pow(log10(1.0 + abs(max_eval / 10)), 2.0) + 1.0;
	// Re: diminuer les valeurs des checks? à voir...
	// Re: augmenter la valeur des pièces?
	//double enlargement_factor = 1.0 + abs(max_eval / 100);
	if (_nodes <= 0) {
		cout << "negative nodes???" << endl;
	}

	// Faut-il chercher plus large en finale?
	//double enlargement_factor = (double)_nodes / (double)_iterations;
	double enlargement_factor = pow((double)_nodes / (double)_iterations / (1 - _board->_adv + 0.5f), 0.3);
	
	// FIXME: quelle est la meilleure manière d'élargir?
	// Réduire beta? Augmenter k_add? Les deux?
	float beta_enlarged = beta / enlargement_factor;
	float k_add_enlarged = k_add * enlargement_factor;

	//cout << "eval: " << abs(max_eval) << ", enlargement factor: " << enlargement_factor << ", beta: " << beta << ", beta_enlarged: " << beta_enlarged << ", k_add: " << k_add << ", k_add_enlarged: " << k_add_enlarged << endl;

	// Softmax sur les évaluations
	softmax(l2, n_children, beta_enlarged, k_add_enlarged);

	//print_array(l2, n_children);

	// *** 2. ***
	// Liste de pondération en fonction de l'exploration de chaque noeud (donne plus de poids aux noeuds moins explorés)
	double pond[100]{};

	i = 0;
	for (auto const& [_, child] : _children) {
		//if (_nodes < 0 || child->_nodes < 0)
		//	cout << "negative nodes!!!!!!!!!!!!!" << endl;
		//if (_nodes == 0 || child->_nodes == 0) {
		//	cout << "0 nodes???" << endl;
		//}
		if (child->_iterations == 0) {
			cout << "0 iterations???" << endl;
		}
		pond[i] = child->_iterations == 0 ? INT_MAX : static_cast<double>(_iterations) / static_cast<double>(child->_iterations);
		i++;
	}

	//print_array(pond, n_children);

	// On applique la pondération aux évaluations
	nodes_weighting(l2, pond, n_children);

	//print_array(l2, n_children);

	// *** 3. ***
	// Somme de toutes les valeurs
	double sum = 0.0;

	for (int i = 0; i < n_children; i++)
	{
		sum += l2[i];
	}

	// Choix du coup en fonction d'une valeur aléatoire
	if (sum > LLONG_MAX) {
		cout << "sum too big" << endl;
	}

	const double rand_val = rand_long(1, sum);
	double cumul = 0.0;

	for (int k = 0; k < n_children; k++)
	{
		cumul += l2[k];
		if (cumul >= rand_val) {
			//cout << "move index: " << i << endl;
			//cout << "move: " << _board->move_label(children_moves[i]) << endl;
			return children_moves[k];
		}
	}

	cout << "first move chosen by default?" << endl;
	print_array(l2, n_children);
	cout << "sum: " << sum << ", rand: " << rand_val << endl;

	return children_moves[0];
}

// Fonction qui renvoie le ratio de victoire pour les blancs du noeud (le ratio de victoire pour une eval alpha est de beta)
double Node::win_ratio(double alpha, double beta) const {
	double eval = _board->_evaluation;
	//cout << "eval: " << eval << ", sigmoid: " << sigmoid(eval, alpha, beta) << endl;
	return sigmoid(eval, alpha, beta);
}

// Fonction qui calcule le score UCT du noeud
double Node::uct_score(int total_iterations, double alpha) const {
	double win_ratio_white = win_ratio();
	double win = !_board->_player ? win_ratio_white : 1 - win_ratio_white;

	//float win = -_board->_evaluation * _board->get_color() / 1000.0f;
	//cout << "eval: " << _board->evaluation_to_string(_board->_evaluation) << ", win_ratio: " << win << endl;
	return win + alpha * sqrt(log(total_iterations) / _iterations);
}

// Fonction qui renvoie le coup avec le meilleur score UCT
Move Node::pick_best_uct_child(float alpha) const {

	// On prend le coup avec le meilleur score UCT
	float best_uct = -FLT_MAX;
	auto best_move = Move();

	//cout << "*** children count: " << children_count() << endl;

	// Pour chaque enfant, on calcule le score UCT
	for (auto const& [move, child] : _children) {
		auto uct = static_cast<float>(child->uct_score(_iterations, alpha));

		//cout << "move: " << _board->move_label(move) << ", uct: " << uct << endl;

		if (uct > best_uct) {
			best_uct = uct;
			best_move = move;
		}
	}

	//cout << "best move: " << _board->move_label(best_move) << endl;

	return best_move;
}


// Fonction qui renvoie le fils le plus exploré
[[nodiscard]] Move Node::get_most_explored_child_move(bool decide_by_eval) {
	int max = 0;

	// Tri simple, on ne départage pas les égalités
	if (!decide_by_eval) {

		auto best_move = Move();

		for (auto const& [move, child] : _children) {
			if (child->_iterations > max) {
				max = child->_iterations;
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
			if (child->_iterations == max) {
				max_iterations_moves.push_back(move);
			}
			else if (child->_iterations > max) {
				max = child->_iterations;
				max_iterations_moves.clear();
				max_iterations_moves.push_back(move);
			}
		}

		// En cas d'égalité, on trie par évaluation
		long long int max_eval = -LLONG_MAX;
		auto best_move = Move();

		for (auto const& move : max_iterations_moves) {
			Node const* child = _children[move];

			if (child->_board->_evaluation * color > max_eval) {
				max_eval = child->_board->_evaluation * color;
				best_move = move;
			}
		}

		return best_move;
	}
}

// Reset le noeud et ses enfants, et les supprime tous
void Node::reset() {
	//cout << "resetting node" << endl;
	_latest_first_move_explored = -1;
	_nodes = 0;
	_iterations = 0;
	_board->reset_board();
	_new_node = true;
	_time_spent = 0;
	_fully_explored = false;
	//cout << "resetting children" << endl;

	for (auto const& [_, child] : _children) {
		child->reset();
	}

	//cout << "clearing children" << endl;
	_children.clear();

	//cout << "done" << endl;
}

// Fonction qui renvoie les variantes d'exploration
string Node::get_exploration_variants(bool main, bool quiescence) {

	// Si on est en fin de variante
	if (_board->_game_over_value) {
		return "";
	}

	string variants = "";

	// S'il y a des coups explorés
	if (children_count() > 0) {

		// Si on est dans le noeud principal, on affiche toutes les variantes
		if (main) {
			// TODO: améliorer ce tri...

			// Trie les enfants par nombre d'itérations par l'algo de GrogrosZero
			vector<pair<int, Move>> children_iterations;

			for (auto const& [move, child] : _children) {
				if (child == nullptr) {
					// FIXME
					cout << "null child" << endl;
					children_iterations.push_back(make_pair(0, move)); // On met un moins pour trier dans l'ordre décroissant
				}
				else {
					children_iterations.push_back(make_pair(-child->_iterations, move)); // On met un moins pour trier dans l'ordre décroissant
				}
			}

			std::ranges::sort(children_iterations.begin(), children_iterations.end());

			// En cas d'égalité, on trie par évaluation
			vector<Move> children_moves;

			vector<pair<int, Move>> children_evaluations;

			int color = _board->get_color();

			children_evaluations.push_back(make_pair(-_children[children_iterations[0].second]->_board->_evaluation * color, children_iterations[0].second));

			for (int i = 1; i < children_count() + 1; i++) {

				// Fin des égalités, on trie par évaluation
				if (i == children_count() || children_iterations[i].first != children_iterations[i - 1].first) {
					std::ranges::sort(children_evaluations.begin(), children_evaluations.end());
					for (auto const& [eval, move] : children_evaluations) {
						children_moves.push_back(move);
					}

					children_evaluations.clear();

					// Enfant à trier
					if (i < children_count()) {
						children_evaluations.push_back(make_pair(-_children[children_iterations[i].second]->_board->_evaluation * color, children_iterations[i].second));
					}
				}
				else {
					children_evaluations.push_back(make_pair(-_children[children_iterations[i].second]->_board->_evaluation * color, children_iterations[i].second));
				}
			}

			for (int i = 0; i < children_moves.size(); i++) {
				const Move move = children_moves[i];
				Node* child = _children[move];

				const int child_iterations = child->_iterations;
				const bool new_quiescence = !quiescence && child_iterations == 0;

				variants += "eval: " + _board->evaluation_to_string(child->_board->_evaluation) + " | ";
				variants += (new_quiescence ? "(" : "") + to_string(_board->_moves_count) + (_board->_player ? ". " : "... ") + _board->move_label(move, true) + " " + child->get_exploration_variants(false, new_quiescence || quiescence) + (new_quiescence ? ")" : "")+ "\n";

				const int nodes = _nodes;
				const int child_nodes = child->_nodes;
				const int nodes_ratio = _nodes == 0 ? 0 : child_nodes * 100 / nodes;
				if (_nodes == 0) {
					cout << "nodes == 0?? le bug est peut-être ici..." << endl;
				}

				const int iterations = _iterations;
				const int iterations_ratio = _iterations == 0 ? 0 : child_iterations * 100 / iterations;

				variants += "I: " + int_to_round_string(child_iterations) + " (" + int_to_round_string(iterations_ratio) + "%) | N: " + int_to_round_string(child_nodes) + " (" + int_to_round_string(nodes_ratio) + "%) | D: " + int_to_round_string(child->get_main_depth() + 1) + " | T: " + clock_to_string(child->_time_spent, true) + "\n\n";
			}
		}

		// Sinon, on affiche seulement le coup le plus exploré
		else {
			// Affiche seulement le premier coup (le plus exploré, et en cas d'égalité, celui avec la meilleure évaluation)
			const Move best_move = get_most_explored_child_move();

			const bool new_quiescence = !quiescence && _children[best_move]->_iterations == 0;

			variants += (new_quiescence ? "(" + _board->evaluation_to_string(_board->_evaluation) + ") (" : "") + (_board->_player ? to_string(_board->_moves_count) + ". " : "") + _board->move_label(best_move, true) + " " + _children[best_move]->get_exploration_variants(false, new_quiescence || quiescence) + (new_quiescence ? ")" : "");
		}
		
	}

	// S'il n'y a pas de coups explorés
	else {

		// On affiche l'évaluation du plateau en fin de variante
		variants = "(" + _board->evaluation_to_string(_board->_evaluation) + ")";
	}
	
	return variants;
}

// Fonction qui renvoie la profondeur de la variante principale
[[nodiscard]] int Node::get_main_depth() {
	if (children_count() > 0) {
		Move main_move = get_most_explored_child_move();

		if (main_move.is_null_move()) {
			cout << "most_explored move is null???" << endl;
			return 0;
		}

		Node* main_child = _children[main_move];

		return main_child->get_main_depth() + 1;
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

// Fonction qui renvoie le meilleur coup
[[nodiscard]] Move Node::get_best_move() {
	return get_most_explored_child_move();
}

// Fonction qui renvoie le fils le plus exploré
[[nodiscard]] Node* Node::get_most_explored_child(bool decide_by_eval) {
	return _children[get_most_explored_child_move(decide_by_eval)];
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
int Node::grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth, int alpha, int beta, Network* network) {
	// TODO: comment gérer la profondeur? faire en fonction de l'importance de la branche?
	// mettre aucune profondeur limite?
	// pourquoi en endgame ça va si loin? il fait full échecs...

	// YA DES BUGS DE PRUNING...
	//r1bqr2k/1pp2p1B/p3p2Q/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 3 26
	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : #2......
	//r1bqr1k1/1pp2p1Q/p3p1B1/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 b - - 6 27 ... ? il regarde pas plus loin?? il affiche pas #1 comme éval...

	//cout << "depth: " << depth << endl;
	//rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3

	// On a au moins évalué le plateau du noeud
	if (_nodes > 0) {
		//cout << "quiescence nodes from already explored:" << _nodes << endl;
	}
	else {
		_nodes = 1;
	}

	// Temps de calcul
	const clock_t begin_monte_time = clock();

	//cout << _new_node << ", " << _board->_evaluated << ", " << (int)_board->_got_moves << ", " << (int)_board->_sorted_moves << ", " << _board->_game_over_checked << endl;

	// Si c'est un nouveau noeud, on fait son initialisation
	if (_new_node) {

		// Vérifie si la partie est finie (normalement, c'est déjà fait dans l'évaluation)
		_board->is_game_over();
		_new_node = false;

		if (_board->_got_moves == -1) {
			_board->get_moves();
		}

		// Trie les coups si ça n'est pas déjà fait (les trie de façon rapide)
		!_board->_sorted_moves && _board->sort_moves();
	}

	// Couleur du joueur
	int color = _board->get_color();

	// Si la partie est finie
	if (_board->is_game_over()) {
		if (_board->_game_over_value == 2)
			_board->_evaluation = 0;
		else {
			_board->_evaluation = (-mate_value + _board->_moves_count * mate_ply) * color;
		}

		_board->_evaluated = true;
		_board->_static_evaluation = _board->_evaluation;

		//_nodes++; // BOF... FIXME
		_nodes = 1;
		_iterations = 1;
		_time_spent += clock() - begin_monte_time;
		//cout << "game over" << endl;

		_fully_explored = true;

		return _board->_evaluation * color;
	}

	// Évalue la position 
	// FIXME: ça arrive souvent?
	if (_board->_evaluated) {
		// FIXME: pourquoi on fait ça déjà??

		//cout << "done" << endl;

		// Position déjà évaluée: on reprend l'évaluation statique du plateau
		_board->_evaluation = _board->_static_evaluation;
	}
	else {
		//cout << "redo" << endl;
		_board->evaluate(eval, false, network, false);
	}

	int stand_pat = _board->_evaluation * color;

	// Si on est en échec (pour ne pas terminer les variantes sur un échec)
	bool in_check = _board->in_check();
	//_move.display();
	//cout << "depth: " << depth << ", in check : " << check_extension << endl;
	//bool check_extension = false;

	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : le check extension fonctionne pas?

	// Stand pat
	if (depth <= 0 && !in_check) {
		_time_spent += clock() - begin_monte_time;
		//cout << "stand pat" << endl;
		return stand_pat;
	}

	//cout << "coucou" << endl;

	// Beta cut-off 
	// FIXME: ça casse un peu tout
	bool test_full_checks = false;

	if (stand_pat >= beta && (!in_check || !test_full_checks)) {
		_time_spent += clock() - begin_monte_time;
		//cout << "beta cut-off1: " << stand_pat << " >= " << beta << endl;
		return beta;
	}
	else {
		//cout << "no beta cut-off1: " << stand_pat << " < " << beta << endl;
	}

	// Mise à jour de alpha si l'éval statique est plus grande
	if (stand_pat > alpha && !in_check) {
	//if (stand_pat > alpha) {
		alpha = stand_pat;
	}


	// Regarde toutes les captures
	// FIXME: faudra regarder que les coups non explorés? (ou gérer avec la profondeur...)
	// TODO: get_next_capture()?
	for (int i = 0; i < _board->_got_moves; i++) {

		// Coup
		const Move move = _board->_moves[i];

		// Ce coup a-t-il déjà été exploré?
		//int child_index = get_child_index(move);
		//bool already_explored = child_index != -1;

		bool already_explored = _children.contains(move);

		//if (already_explored) {
		//	cout << "move already explored" << endl;
		//	// FIXME: IL FAUT GERER CE CAS-LA!!!
		//	continue;
		//}

		// Doit-on explorer ce coup?
		bool should_explore = false;

		// Si on est en échec, on explore tous les coups
		if (in_check) {
			should_explore = true;
		}
		else {
			// Si c'est une capture
			if (_board->_array[move.i2][move.j2] != 0) {
				should_explore = true;
			}
			// Si c'est un roque (EXPÉRIMENTAL)
			//else if ((_board->_array[move.i1][move.j1] == 6 && abs(move.j1 - move.j2) == 2) || (_board->_array[move.i1][move.j1] == 12 && abs(move.j1 - move.j2) == 2)) {
			//	should_explore = true;
			//}
			else {
				// Si c'est une promotion
				if ((_board->_array[move.i1][move.j1] == 1 && move.i2 == 7) || (_board->_array[move.i1][move.j1] == 7 && move.i2 == 0)) {
					should_explore = true;
				}
				// Si le coup met en échec
				else {
					Board b(*_board);
					b.make_move(move);
					should_explore = b.in_check();
				}
			}
		}

		/*if (should_explore)
			cout << "depth: " << depth << " | move: " << _board->move_label(move) << " | check_extension: " << check_extension << endl;*/

		// *** EXPLORATION ***
		// Si c'est une capture/échec/promotion, on explore
		if (should_explore) {

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

			

			//cout << "beta: " << beta << " | alpha: " << alpha << endl;

			// Appel récursif sur le fils
			int score = - child->grogros_quiescence(buffer, eval, depth - 1, -beta, -alpha, network);
			//child->grogros_quiescence(buffer, eval, depth - 1);
			_nodes += child->_nodes;

			// Mise à jour de l'évaluation du plateau
			// FIXME: ici, s'il y'a un seul coup, il faut mettre à jour l'évaluation du plateau même si l'évaluation fils est moins bonne
			// Ou alors: si tous les coups ont été explorés, on met à jour l'évaluation du plateau avec le meilleur coup
			//bool all_moves_explored = get_fully_explored_children_count() == _board->_got_moves;
			bool all_moves_explored = children_count() == _board->_got_moves;

			if (all_moves_explored) {
				int best_eval = -INT_MAX;

				for (auto const& [move_2, child_2] : _children) {
					if (child_2->_board->_evaluation * color > best_eval) {
						best_eval = child_2->_board->_evaluation * color;
					}
				}

				_board->_evaluation = best_eval * color;
			}
			else {
				if (child->_board->_evaluation * color > _board->_evaluation * color) {
					_board->_evaluation = child->_board->_evaluation;
				}
			}

			// Beta cut-off
			if (score >= beta) {
				_time_spent += clock() - begin_monte_time;
				//cout << "move: " << _board->move_label(move) << " | beta cut-off2: " << score << " >= " << beta << endl;
				return beta;
			}

			// Mise à jour de alpha si l'éval statique est plus grande
			if (score > alpha) {
				alpha = score;
			}

			
		}
	}

	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	//cout << "alpha: " << alpha << endl;
	//_board->_evaluation = alpha * color;

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

// Tentative de quiescence maison
void Node::new_grogros_quiescence(Buffer* buffer, Evaluator* eval, float beta, float k_add, int nodes) {

	// *** IDEE DE L'ALGO ***
	// On ne regarde que les captures (pas les échecs ni promotions pour le moment...)
	// On les trie (de la meilleure à la pire)
	// On les joue une par une (tant qu'il y en a)
	// Quand on les a toutes explorées, on regarde plus profondément les meilleures
	// On s'arrête quand y'a plus de captures ou qu'on a plus de noeuds



	// Temps de calcul
	const clock_t begin_monte_time = clock();

	// Si c'est un nouveau noeud, on fait son initialisation
	if (_new_node) {

		// Vérifie si la partie est finie (normalement, c'est déjà fait dans l'évaluation)
		_board->is_game_over();
		_new_node = false;

		if (_board->_got_moves == -1) {
			_board->get_moves();
		}

		// Trie les coups si ça n'est pas déjà fait (les trie de façon rapide)
		!_board->_sorted_moves && _board->sort_moves();
	}

	// Si la partie est finie, on ne fait rien
	if (_board->_game_over_value) {
		// TODO: on fait quoi là??
		_nodes++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// Vérifie que le buffer n'est pas plein (TODO) (et est bien initialisé aussi)
	

	// Exploration des noeuds
	while (nodes > 0) {
		int initial_nodes = _nodes;

		// S'il reste des coups à explorer
		/*if (children_count() < _board->_got_moves) {
			explore_new_move(buffer, eval, quiescence_depth);
		}*/

		//cout << "explored nodes: " << get_fully_explored_children_count() << " | got moves: " << (int)_board->_got_moves << endl;

		if (get_fully_explored_children_count() < _board->_got_moves) {
			//explore_new_capture(buffer, eval);
		}

		// On explore dans un fils
		else {
			//explore_random_capture(buffer, eval, beta, k_add);
		}

		nodes -= _nodes - initial_nodes;
	}
	
	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	return;

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