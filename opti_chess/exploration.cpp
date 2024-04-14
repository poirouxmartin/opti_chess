#include "exploration.h"
#include "useful_functions.h"

// Constructeur avec un plateau, un indice et un coup
Node::Node(Board *board, Move move) {
	_board = board;
	_move = move;
}

// Fonction qui ajoute un fils
void Node::add_child(Node* child) {
	// FIXME: vérifier si le coup n'est pas déjà dans les enfants?
	_children.push_back(child);
}

// Fonction qui renvoie le nombre de fils
[[nodiscard]] int Node::children_count() const {
	return _children.size();
}

// Fonction qui renvoie le numéro du fils associé au coup s'il existe, -1 sinon
[[nodiscard]] int Node::get_child_index(Move move) const {
	for (int i = 0; i < _children.size(); i++) {
		if (_children[i]->_move == move) {
			return i;
		}
	}

	return -1;
}

// Fonction qui renvoie l'indice du premier coup qui n'a pas encore été ajouté, -1 sinon
[[nodiscard]] int Node::get_first_unexplored_move_index() {
	for (int i = _latest_first_move_explored + 1; i < _board->_got_moves; i++) {
		if (get_child_index(_board->_moves[i]) == -1) {
			_latest_first_move_explored = i;
			return i;
		}
	}

	return -1;
}

// Nouveau GrogrosZero
void Node::grogros_zero(Buffer* buffer, Evaluator* eval, float beta, float k_add, int nodes, int quiescence_depth) {
	// TODO:
	// On peut rajouter la profondeur
	// Garder le temps de calcul

	// BUG: rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1
	// quiescence là dessus, après il regarde à donf Tb1 après Dxh7... ??

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
		if (children_count() < _board->_got_moves) {
			explore_new_move(buffer, eval, quiescence_depth);
		}

		// On explore dans un fils
		else {
			explore_random_child(buffer, eval, beta, k_add, quiescence_depth);
		}

		nodes -= _nodes - initial_nodes;
	}
	
	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	return;
}

// Fonction qui explore un nouveau coup
void Node::explore_new_move(Buffer* buffer, Evaluator* eval, int quiescence_depth) {
	// On prend le premier coup non exploré
	const int move_index = get_first_unexplored_move_index();
	Move move = _board->_moves[move_index];

	// Prend une place dans le buffer
	const int buffer_index = buffer->get_first_free_index();

	// FIXME: faut peut-être check ça autre part...
	if (buffer_index == -1) {
		cout << "Buffer is full" << endl;
		return;
	}

	Board *new_board = &buffer->_heap_boards[buffer_index];
	new_board->copy_data(*_board, false, true);
	new_board->_is_active = true;
	new_board->make_move(move, false, false, true);

	// Création du noeud fils
	Node* child = new Node(new_board, move);

	// Evalue le plateau, en regardant si la partie est finie
	//new_board->evaluate(eval, false, nullptr, true);
	//new_board->_evaluation = new_board->quiescence(eval, -INT32_MAX, INT32_MAX, 4, true) * new_board->get_color();
	child->grogros_quiescence(buffer, eval, quiescence_depth);
	bool test = false;
	// rnb1kbnr/ppp1pppp/2q5/1B6/8/2N5/PPPP1PPP/R1BQK1NR b KQkq - 3 4
	// rnb1kbnr/ppp1pppp/2q5/8/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4 : ici Fb5 -> +114 au lieu de +895

	// Met à jour l'évaluation du plateau
	if (children_count() == 0 && test) { // Si c'est désactivé, alors il ne vera sûrement pas les zugzwang. Mais si activé, s'il joue un mauvais coup, il va considérer la branche comme mauvaise
		_board->_evaluation = new_board->_evaluation; // Pour le quiescence search, faut-il ne pas aller là pour garder un standpat?
		//cout << "eval: " << _board->evaluation_to_string(_board->_evaluation) << endl;
	}
	else {
		// TODO: c'est vraiment ça qu'il faut faire?
		_board->_evaluation = _board->_player * max(_board->_evaluation, new_board->_evaluation) + !_board->_player * min(_board->_evaluation, new_board->_evaluation);
	}
	

	// FIXME: si on a regardé tous les fils, et qu'aucun des coups n'améliore l'évaluation, on fait quoi?
	// - Option 1: on garde l'évaluation sans aucun coup
	// - Option 2: on garde l'évaluation du meilleur coup
	// Est-ce vraiment grave? peut-être pas car si on continue une profondeur plus loin, explore_random_child() va prendre le meilleur coup

	// Augmente le nombre de noeuds
	//_nodes++;
	_nodes += child->_nodes;

	// Ajoute le fils
	//Node* child = new Node(new_board, move);
	////child->_nodes = 1; // FIXME: 0?
	add_child(child);	
}

// Fonction qui explore dans un plateau fils pseudo-aléatoire
void Node::explore_random_child(Buffer* buffer, Evaluator* eval, float beta, float k_add, int quiescence_depth) {
	// Prend un fils aléatoire
	int child_index = pick_random_child_index(beta, k_add);

	_nodes -= _children[child_index]->_nodes; // On enlève le nombre de noeuds de ce fils

	// On explore ce fils
	_children[child_index]->grogros_zero(buffer, eval, beta, k_add, 1, quiescence_depth); // L'évaluation du fils est mise à jour ici

	// Met à jour l'évaluation du plateau
	if (_board->_player) {
		_board->_evaluation = -INT_MAX;
		for (int i = 0; i < children_count(); i++) {
			_board->_evaluation = max(_board->_evaluation, _children[i]->_board->_evaluation);
		}
	}
	else {
		_board->_evaluation = INT_MAX;
		for (int i = 0; i < children_count(); i++) {
			_board->_evaluation = min(_board->_evaluation, _children[i]->_board->_evaluation);
		}
	}


	// Augmente le nombre de noeuds
	//_nodes++;
	_nodes += _children[child_index]->_nodes;
}

// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variantes, en fonction de la grandeur du nombre correspondant à cet index
int Node::pick_random_child_index(const float beta, const float k_add) {
	int color = _board->get_color();
	int n_children = children_count();

	// *** 1. ***
	// Evaluations des enfants (en fonction de la couleur)
	int l2[100];

	for (int i = 0; i < n_children; i++)
		l2[i] = color * _children[i]->_board->_evaluation;

	// Softmax sur les évaluations
	softmax(l2, n_children, beta, k_add);


	// *** 2. ***
	// Liste de pondération en fonction de l'exploration de chaque noeud (donne plus de poids aux noeuds moins explorés)
	float pond[100];

	for (int i = 0; i < n_children; i++)
		pond[i] = static_cast<float>(_nodes) / static_cast<float>(_children[i]->_nodes);

	// On applique la pondération aux évaluations
	nodes_weighting(l2, pond, n_children);

	// *** 3. ***
	// Somme de toutes les valeurs
	int sum = 0;

	for (int i = 0; i < n_children; i++)
	{
		sum += l2[i];
	}

	// Choix du coup en fonction d'une valeur aléatoire
	const int rand_val = rand_int(1, sum);
	int cumul = 0;

	for (int i = 0; i < n_children; i++)
	{
		cumul += l2[i];
		if (cumul >= rand_val)
			return i;
	}

	return 0;
}

// Fonction qui renvoie le fils le plus exploré
[[nodiscard]] int Node::get_most_explored_child_index(bool decide_by_eval) {
	int max = 0;

	// Tri simple, on ne départage pas les égalités
	if (!decide_by_eval) {

		int index = 0;

		for (int i = 0; i < children_count(); i++) {
			if (_children[i]->_nodes > max) {
				max = _children[i]->_nodes;
				index = i;
			}
		}

		return index;
	}

	// Avec un départage par égalité
	else {
		// TODO: c'est foireux...
		vector<int> indices;
		int color = _board->get_color();

		for (int i = 0; i < children_count(); i++) {
			if (_children[i]->_nodes == max) {
				indices.push_back(i);
			}
			else if (_children[i]->_nodes > max) {
				max = _children[i]->_nodes;
				indices.clear();
				indices.push_back(i);
			}
		}

		// En cas d'égalité, on trie par évaluation
		int max_eval = -INT_MAX;
		int index = 0;

		for (int i = 0; i < indices.size(); i++) {
			if (_children[indices[i]]->_board->_evaluation * color > max_eval) {
				max_eval = _children[indices[i]]->_board->_evaluation * color;
				index = indices[i];
			}
		}

		return index;
	}

	
}

// Reset le noeud et ses enfants, et les supprime tous
void Node::reset() {
	_latest_first_move_explored = -1;
	_nodes = 0;
	_board->reset_board();
	_move = Move();
	_new_node = true;
	_time_spent = 0;

	for (int i = 0; i < children_count(); i++) {
		_children[i]->reset();
		delete _children[i];
	}

	_children.clear();
}

// Fonction qui renvoie les variantes d'exploration
string Node::get_exploration_variants(bool main) {

	// TODO:
	// Afficher les icônes des pièces (♔, ♕, ♖, ♗, ♘, ♙, ♚, ♛, ♜, ♝, ♞, ♟) (UTF-8)

	// Si on est en fin de variante
	if (_board->_game_over_value) {
		return "";
	}

	string variants = "";

	// S'il y a des coups explorés
	if (children_count() > 0) {

		// Si on est dans le noeud principal, on affiche toutes les variantes
		if (main) {
			// Trie les enfants par nombre de noeuds
			vector<pair<int, int>> children_nodes;
			for (int i = 0; i < children_count(); i++) {
				children_nodes.push_back(make_pair(-_children[i]->_nodes, i)); // On met un moins pour trier dans l'ordre décroissant
			}

			sort(children_nodes.begin(), children_nodes.end());

			// En cas d'égalité, on trie par évaluation
			vector<int> children_index;

			vector<pair<int, int>> children_evaluations;

			int previous_nodes = children_nodes[0].first;
			int color = _board->get_color();

			children_evaluations.push_back(make_pair(- _children[children_nodes[0].second]->_board->_evaluation * color, children_nodes[0].second));

			for (int i = 1; i < children_count() + 1; i++) {

				// Fin des égalités, on trie par évaluation
				if (i == children_count() || children_nodes[i].first != children_nodes[i - 1].first) {
					sort(children_evaluations.begin(), children_evaluations.end());
					for (int j = 0; j < children_evaluations.size(); j++) {
						children_index.push_back(children_evaluations[j].second);
					}

					children_evaluations.clear();

					// Enfant à trier
					if (i < children_count()) {
						children_evaluations.push_back(make_pair(- _children[children_nodes[i].second]->_board->_evaluation * color, children_nodes[i].second));
					}
				}
				else {
					children_evaluations.push_back(make_pair(- _children[children_nodes[i].second]->_board->_evaluation * color, children_nodes[i].second));
				}
			}


			for (int i = 0; i < children_count(); i++) {
				int child_index = children_index[i];

				variants += "eval: " + _board->evaluation_to_string(_children[child_index]->_board->_evaluation) + " | ";
				variants += to_string(_board->_moves_count) + (_board->_player ? ". " : "... ") + _board->move_label(_children[child_index]->_move) + " " + _children[child_index]->get_exploration_variants(false) + "\n";
				variants += "N: " + int_to_round_string(_children[child_index]->_nodes) + " (" + int_to_round_string(_children[child_index]->_nodes * 100 / _nodes) + "%) | D: " + int_to_round_string(_children[child_index]->get_main_depth() + 1) + " | T: " + clock_to_string(_children[child_index]->_time_spent) + "s\n\n";

			}
		}

		// Sinon, on affiche seulement le coup le plus exploré
		else {
			// Affiche seulement le premier coup (le plus exploré, et en cas d'égalité, celui avec la meilleure évaluation)
			int index_best_move = get_most_explored_child_index();

			variants += (_board->_player ? to_string(_board->_moves_count) + ". " : "") + _board->move_label(_children[index_best_move]->_move) + " " + _children[index_best_move]->get_exploration_variants(false);
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
	int depth = 0;

	Node* current_node = this;

	while (current_node->children_count() > 0) {
		current_node = current_node->_children[current_node->get_most_explored_child_index()];
		depth++;
	}

	return depth;
}

// Destructeur
Node::~Node() {
	/*for (int i = 0; i < children_count(); i++) {
		delete _children[i];
	}*/
}

// Fonction qui renvoie le meilleur coup
[[nodiscard]] Move Node::get_best_move() {
	int index_best_move = get_most_explored_child_index();
	return _children[index_best_move]->_move;
}

// Fonction qui renvoie le fils le plus exploré
[[nodiscard]] Node* Node::get_most_explored_child(bool decide_by_eval) {
	int index_best_move = get_most_explored_child_index(decide_by_eval);
	return _children[index_best_move];
}

// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
[[nodiscard]] int Node::get_avg_nps() const {
	return _time_spent == 0 ? 0 : (_nodes / _time_spent * CLOCKS_PER_SEC);
}

// Quiescence search intégré à l'exploration
int Node::grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth, int alpha, int beta) {
	// YA DES BUGS DE PRUNING...
	//r1bqr2k/1pp2p1B/p3p2Q/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 3 26
	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : #2......
	//r1bqr1k1/1pp2p1Q/p3p1B1/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 b - - 6 27 ... ? il regarde pas plus loin??

	//cout << "depth: " << depth << endl;
	//rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3

	// On a au moins évalué le plateau du noeud
	_nodes = 1;

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

	// Couleur du joueur
	int color = _board->get_color();

	// Si la partie est finie
	if (_board->is_game_over()) {
		if (_board->_game_over_value == 2)
			_board->_evaluation = 0;
		else
			_board->_evaluation = (-mate_value + _board->_moves_count * mate_ply) * color;

		_nodes++; // BOF
		_time_spent += clock() - begin_monte_time;
		//cout << "game over" << endl;

		return _board->_evaluation * color;
	}

	// Évalue la position
	_board->evaluate(eval);
	int stand_pat = _board->_evaluation * color;

	// Si on est en échec (pour ne pas terminer les variantes sur un échec)
	bool check_extension = _board->in_check();
	//cout << "check: " << check_extension << endl;
	//bool check_extension = false;

	// Stand pat
	if (depth <= 0 && !check_extension) {
		_time_spent += clock() - begin_monte_time;
		//cout << "stand pat" << endl;
		return stand_pat;
	}

	//cout << "coucou" << endl;

	// Beta cut-off 
	// FIXME: ça casse un peu tout
	if (stand_pat >= beta) {
		_time_spent += clock() - begin_monte_time;
		//cout << "beta cut-off1: " << stand_pat << " >= " << beta << endl;
		return beta;
	}
	else {
		//cout << "no beta cut-off1: " << stand_pat << " < " << beta << endl;
	}

	// Mise à jour de alpha si l'éval statique est plus grande
	if (stand_pat > alpha && !check_extension) {
		alpha = stand_pat;
	}


	// Regarde toutes les captures
	// FIXME: faudra regarder que les coups non explorés? (ou gérer avec la profondeur...)
	// TODO: get_next_capture()?
	for (int i = 0; i < _board->_got_moves; i++) {

		// Coup
		Move move = _board->_moves[i];

		// Ce coup a-t-il déjà été exploré?
		if (get_child_index(move) != -1) {
			cout << "move already explored" << endl;
			continue;
		}

		// Doit-on explorer ce coup?
		bool should_explore = false;

		// Si on est en échec, on explore tous les coups
		if (check_extension) {
			should_explore = true;
		}
		else {
			// Si c'est une capture
			if (_board->_array[move.i2][move.j2] != 0) {
				should_explore = true;
			}
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
			Node* child = new Node(new_board, move);
			add_child(child);

			//cout << "beta: " << beta << " | alpha: " << alpha << endl;

			// Appel récursif sur le fils
			int score = - child->grogros_quiescence(buffer, eval, depth - 1, -beta, -alpha);
			//child->grogros_quiescence(buffer, eval, depth - 1);
			_nodes += child->_nodes;

			// Mise à jour de l'évaluation du plateau
			if (child->_board->_evaluation * color > _board->_evaluation * color) {
				_board->_evaluation = child->_board->_evaluation;
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

	return alpha;
}