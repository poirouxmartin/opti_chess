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

	_children[move] = child;
}

// Fonction qui renvoie le nombre de fils
[[nodiscard]] int Node::children_count() const {
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
void Node::grogros_zero(Buffer* buffer, Evaluator* eval, float beta, float k_add, int nodes, int quiescence_depth, Network* network) {
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

		if (get_fully_explored_children_count() < _board->_got_moves) {
			explore_new_move(buffer, eval, quiescence_depth, network);
		}

		// On explore dans un fils
		else {
			explore_random_child(buffer, eval, beta, k_add, quiescence_depth, network);
		}

		nodes -= _nodes - initial_nodes;
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
		// Ca marche pas forcément, car le coup parent ne sera pas le même, et ça fait tout bugger...
		// Faire des noeud jumeaux? pas forcément le mieux...
		// Stocker les parents? et/ou les différents coups par parent?
		// Quand on update un des noeuds, il faudra potentiellement backpropagate dans toutes les branches parentes...

		// TODO: à la place se stocker le coup joué dans le fils, faire un dictionnaire des fils avec leur coup associé?

		// Si la position est déjà dans la table de transposition
		if (transposition_table.contains(new_board->_zobrist_key)) {
			cout << "transposition: "<< new_board->to_fen() << endl;

			child = transposition_table._hash_table[new_board->_zobrist_key]._node;
			
			//cout << "nodes: " << child->_nodes;

			_nodes += child->_nodes;
		}
		else {
			// Création du noeud fils
			child = new Node(new_board);

			// Ajoute la position dans la table
			//transposition_table._hash_table[new_board->_zobrist_key] = ZobristEntry(child);

			//cout << "normal" << endl;
		}
	}

	// Evalue le plateau, en regardant si la partie est finie
	//child->_board->evaluate(eval, false, nullptr, true);
	//child->_board->_evaluation = child->_board->quiescence(eval, -INT32_MAX, INT32_MAX, 4, true) * child->_board->get_color();
	//child->_nodes = 1;

	if (!child->_fully_explored) {
		child->grogros_quiescence(buffer, eval, quiescence_depth, -INT32_MAX, INT32_MAX, network);
		//child->_board->evaluate(eval, false, nullptr, true);
		//child->_nodes = 1;

		child->_fully_explored = true;
	}

	bool test = false;
	// rnb1kbnr/ppp1pppp/2q5/1B6/8/2N5/PPPP1PPP/R1BQK1NR b KQkq - 3 4
	// rnb1kbnr/ppp1pppp/2q5/8/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4 : ici Fb5 -> +114 au lieu de +895

	// Met à jour l'évaluation du plateau
	if (children_count() == 0 && test) { // Si c'est désactivé, alors il ne vera sûrement pas les zugzwang. Mais si activé, s'il joue un mauvais coup, il va considérer la branche comme mauvaise
		_board->_evaluation = child->_board->_evaluation; // Pour le quiescence search, faut-il ne pas aller là pour garder un standpat?
		//cout << "eval: " << _board->evaluation_to_string(_board->_evaluation) << endl;
	}
	else {
		// TODO: c'est vraiment ça qu'il faut faire?
		_board->_evaluation = _board->_player * max(_board->_evaluation, child->_board->_evaluation) + !_board->_player * min(_board->_evaluation, child->_board->_evaluation);
	}

	// FIXME optimiser ces cas pour n'en calculer qu'un

	// Si tous les coups ont été explorés, on met à jour l'évaluation du plateau avec le meilleur coup
	if (children_count() == _board->_got_moves) {
		int color = _board->get_color();
		int best_eval = -INT_MAX;

		for (auto& [move, child] : _children) {
			if (child->_board->_evaluation * color > best_eval) {
				best_eval = child->_board->_evaluation * color;
			}
		}

		_board->_evaluation = best_eval * color;
	}
	

	// FIXME: si on a regardé tous les fils, et qu'aucun des coups n'améliore l'évaluation, on fait quoi?
	// - Option 1: on garde l'évaluation sans aucun coup
	// - Option 2: on garde l'évaluation du meilleur coup
	// Est-ce vraiment grave? peut-être pas car si on continue une profondeur plus loin, explore_random_child() va prendre le meilleur coup

	// Augmente le nombre de noeuds
	//_nodes++;
	_nodes += child->_nodes;

	// Ajoute le fils
	if (!already_explored) {
		add_child(child, move);
	}
}

// Fonction qui explore dans un plateau fils pseudo-aléatoire
void Node::explore_random_child(Buffer* buffer, Evaluator* eval, float beta, float k_add, int quiescence_depth, Network* network) {

	// Prend un fils aléatoire
	const Move move = pick_random_child(beta, k_add);
	
	//cout << "move: " << _board->move_label(move) << endl;

	_nodes -= _children[move]->_nodes; // On enlève le nombre de noeuds de ce fils

	// On explore ce fils
	_children[move]->grogros_zero(buffer, eval, beta, k_add, 1, quiescence_depth, network); // L'évaluation du fils est mise à jour ici

	// Met à jour l'évaluation du plateau
	if (_board->_player) {
		_board->_evaluation = -INT_MAX;

		for (auto& [move, child] : _children) {
			_board->_evaluation = max(_board->_evaluation, child->_board->_evaluation);
		}
	}
	else {
		_board->_evaluation = INT_MAX;

		for (auto& [move, child] : _children) {
			_board->_evaluation = min(_board->_evaluation, child->_board->_evaluation);
		}
	}

	// Augmente le nombre de noeuds
	_nodes += _children[move]->_nodes;
}

// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variantes, en fonction de la grandeur du nombre correspondant à cet index
Move Node::pick_random_child(const float beta, const float k_add) {
	int color = _board->get_color();
	int n_children = children_count();

	// *** 1. ***
	// Evaluations des enfants (en fonction de la couleur)
	int l2[100]{};
	map<int, Move> children_moves;

	int i = 0;
	for (auto& [move, child] : _children) {
		l2[i] = color * child->_board->_evaluation;
		children_moves[i] = move;
		i++;
	}

	//print_array(l2, n_children);

	// Softmax sur les évaluations
	softmax(l2, n_children, beta, k_add);

	//print_array(l2, n_children);

	// *** 2. ***
	// Liste de pondération en fonction de l'exploration de chaque noeud (donne plus de poids aux noeuds moins explorés)
	float pond[100]{};

	i = 0;
	for (auto& [move, child] : _children) {
		pond[i] = static_cast<float>(_nodes) / static_cast<float>(child->_nodes);
		i++;
	}

	//print_array(pond, n_children);

	// On applique la pondération aux évaluations
	nodes_weighting(l2, pond, n_children);

	//print_array(l2, n_children);

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
			return children_moves[i];
	}

	return children_moves[0];
}

// Fonction qui renvoie un fils de manière pseudo-aléatoire, en fonction de l'évaluation et du nombre de noeuds
Move Node::pick_random_child_new() {
	// TODO
	return Move();
}


// Fonction qui renvoie le fils le plus exploré
[[nodiscard]] Move Node::get_most_explored_child_move(bool decide_by_eval) {
	int max = 0;

	// Tri simple, on ne départage pas les égalités
	if (!decide_by_eval) {

		Move best_move = Move();

		for (auto& [move, child] : _children) {
			if (child->_nodes > max) {
				max = child->_nodes;
				best_move = move;
			}
		}

		return best_move;
	}

	// Avec un départage par égalité
	else {
		vector<Move> max_nodes_moves;
		int color = _board->get_color();

		for (auto& [move, child] : _children) {
			if (child->_nodes == max) {
				max_nodes_moves.push_back(move);
			}
			else if (child->_nodes > max) {
				max = child->_nodes;
				max_nodes_moves.clear();
				max_nodes_moves.push_back(move);
			}
		}

		// En cas d'égalité, on trie par évaluation
		int max_eval = -INT_MAX;
		Move best_move = Move();

		for (int i = 0; i < max_nodes_moves.size(); i++) {
			Move move = max_nodes_moves[i];
			Node* child = _children[move];

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
	_latest_first_move_explored = -1;
	_nodes = 0;
	_board->reset_board();
	_new_node = true;
	_time_spent = 0;
	_fully_explored = false;


	for (auto& [move, child] : _children) {
		child->reset();
		delete child;
	}

	_children.clear();
}

// Fonction qui renvoie les variantes d'exploration
string Node::get_exploration_variants(bool main) {

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
			vector<pair<int, Move>> children_nodes;

			for (auto& [move, child] : _children) {
				children_nodes.push_back(make_pair(-child->_nodes, move)); // On met un moins pour trier dans l'ordre décroissant
			}

			sort(children_nodes.begin(), children_nodes.end());

			// En cas d'égalité, on trie par évaluation
			vector<Move> children_moves;

			vector<pair<int, Move>> children_evaluations;

			int previous_nodes = children_nodes[0].first;
			int color = _board->get_color();

			children_evaluations.push_back(make_pair(- _children[children_nodes[0].second]->_board->_evaluation * color, children_nodes[0].second));

			for (int i = 1; i < children_count() + 1; i++) {

				// Fin des égalités, on trie par évaluation
				if (i == children_count() || children_nodes[i].first != children_nodes[i - 1].first) {
					sort(children_evaluations.begin(), children_evaluations.end());
					for (int j = 0; j < children_evaluations.size(); j++) {
						children_moves.push_back(children_evaluations[j].second);
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

			for (int i = 0; i < children_moves.size(); i++) {
				Move move = children_moves[i];
				Node* child = _children[move];

				variants += "eval: " + _board->evaluation_to_string(child->_board->_evaluation) + " | ";
				variants += to_string(_board->_moves_count) + (_board->_player ? ". " : "... ") + _board->move_label(move, true) + " " + child->get_exploration_variants(false) + "\n";
				variants += "N: " + int_to_round_string(child->_nodes) + " (" + int_to_round_string(child->_nodes * 100 / _nodes) + "%) | D: " + int_to_round_string(child->get_main_depth() + 1) + " | T: " + clock_to_string(child->_time_spent) + "s\n\n";
			}
		}

		// Sinon, on affiche seulement le coup le plus exploré
		else {
			// Affiche seulement le premier coup (le plus exploré, et en cas d'égalité, celui avec la meilleure évaluation)
			Move best_move = get_most_explored_child_move();

			variants += (_board->_player ? to_string(_board->_moves_count) + ". " : "") + _board->move_label(best_move, true) + " " + _children[best_move]->get_exploration_variants(false);
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
		current_node = current_node->_children[current_node->get_most_explored_child_move()];
		depth++;
	}

	return depth;
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
	return _time_spent == 0 ? 0 : (_nodes / _time_spent * CLOCKS_PER_SEC);
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
	_nodes = 1;

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
		else
			_board->_evaluation = (-mate_value + _board->_moves_count * mate_ply) * color;

		_board->_evaluated = true;
		_board->_static_evaluation = _board->_evaluation;

		_nodes++; // BOF
		_time_spent += clock() - begin_monte_time;
		//cout << "game over" << endl;

		return _board->_evaluation * color;
	}

	// Évalue la position 
	// FIXME: ça arrive souvent?
	if (_board->_evaluated) {
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
	bool check_extension = _board->in_check();
	//_move.display();
	//cout << "depth: " << depth << ", in check : " << check_extension << endl;
	//bool check_extension = false;

	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : le check extension fonctionne pas?

	// Stand pat
	if (depth <= 0 && !check_extension) {
		_time_spent += clock() - begin_monte_time;
		//cout << "stand pat" << endl;
		return stand_pat;
	}

	//cout << "coucou" << endl;

	// Beta cut-off 
	// FIXME: ça casse un peu tout
	bool test_full_checks = false;

	if (stand_pat >= beta && (!check_extension || !test_full_checks)) {
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
			bool all_moves_explored = children_count() == _board->_got_moves;

			if (all_moves_explored) {
				int color = _board->get_color();
				int best_eval = -INT_MAX;

				for (auto& [move, child] : _children) {
					if (child->_board->_evaluation * color > best_eval) {
						best_eval = child->_board->_evaluation * color;
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

	for (auto& [move, child] : _children) {
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