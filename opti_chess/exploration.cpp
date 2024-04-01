#include "exploration.h"
#include "useful_functions.h"

// Constructeur avec un plateau, un indice et un coup
Node::Node(Board board, int index, Move move) {
	_board = board;
	_index = index;
	_move = move;
}

// Fonction qui ajoute un fils
void Node::add_child(Node* child) {
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
	for (int i = _latest_first_move_explored + 1; i < _board._got_moves; i++) {
		if (get_child_index(_board._moves[i]) == -1) {
			_latest_first_move_explored = i;
			return i;
		}
	}

	return -1;
}

// Nouveau GrogrosZero
void Node::grogros_zero(Buffer buffer, Evaluator eval, float beta, float k_add, int nodes) {
	// TODO:
	// On peut rajouter la profondeur
	// Garder le temps de calcul

	// Si c'est un nouveau noeud, on fait son initialisation
	if (_new_node) {

		// Vérifie si la partie est finie
		_board.is_game_over(); // Calcule les coup possibles au passage
		_new_node = false;

		// Trie les coups si ça n'est pas déjà fait (les trie de façon rapide)
		!_board._sorted_moves && _board.sort_moves();
	}

	// Si la partie est finie, on ne fait rien
	if (_board._game_over_value) {
		// TODO: on fait quoi là??


		_nodes++;
		return;
	}

	// Vérifie que le buffer n'est pas plein (TODO) (et est bien initialisé aussi)
	

	// Exploration des noeuds
	while (nodes > 0) {

		// S'il reste des coups à explorer
		if (children_count() < _board._got_moves) {
			explore_new_move(buffer, eval);
		}

		// On explore dans un fils
		else {
			explore_random_child(buffer, eval, beta, k_add);
		}

		nodes--;
	}
	
}

// Fonction qui explore un nouveau coup
void Node::explore_new_move(Buffer buffer, Evaluator eval) {
	// On prend le premier coup non exploré
	const int move_index = get_first_unexplored_move_index();
	Move move = _board._moves[move_index];

	// Prend une place dans le buffer
	const int buffer_index = buffer.get_first_free_index();
	Board new_board = buffer._heap_boards[buffer_index];
	new_board.copy_data(_board, false, true);
	new_board._is_active = true;
	new_board.make_move(move, false, false, true);

	// Evalue le plateau, en regardant si la partie est finie
	new_board.evaluate(&eval, false, nullptr, true);

	// Met à jour l'évaluation du plateau
	if (children_count() == 0) {
		_board._evaluation = new_board._evaluation; // Pour le quiescence search, faut-il ne pas aller là pour garder un standpat?
	}
	else {
		_board._evaluation = _board._player * max(_board._evaluation, new_board._evaluation) + !_board._player * min(_board._evaluation, new_board._evaluation);
	}

	// Augmente le nombre de noeuds
	_nodes++;

	// Ajoute le fils
	Node* child = new Node(new_board, buffer_index, move);
	child->_nodes = 1;
	add_child(child);	
}

// Fonction qui explore dans un plateau fils pseudo-aléatoire
void Node::explore_random_child(Buffer buffer, Evaluator eval, float beta, float k_add) {
	// Prend un fils aléatoire
	int child_index = pick_random_child_index(beta, k_add);

	// On explore ce fils
	_children[child_index]->grogros_zero(buffer, eval, beta, k_add, 1); // L'évaluation du fils est mise à jour ici
	
	// Met à jour l'évaluation du plateau
	if (_board._player) {
		_board._evaluation = -INT_MAX;
		for (int i = 0; i < children_count(); i++) {
			_board._evaluation = max(_board._evaluation, _children[i]->_board._evaluation);
		}
	}
	else {
		_board._evaluation = INT_MAX;
		for (int i = 0; i < children_count(); i++) {
			_board._evaluation = min(_board._evaluation, _children[i]->_board._evaluation);
		}
	}


	// Augmente le nombre de noeuds
	_nodes++;
}

// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variantes, en fonction de la grandeur du nombre correspondant à cet index
int Node::pick_random_child_index(const float beta, const float k_add) {
	int color = _board.get_color();
	int n_children = children_count();

	// *** 1. ***
	// Evaluations des enfants (en fonction de la couleur)
	int l2[100];

	for (int i = 0; i < n_children; i++)
		l2[i] = color * _children[i]->_board._evaluation;

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

		vector<int> indices;
		int color = _board.get_color();

		for (int i = 0; i < children_count(); i++) {
			if (_children[i]->_nodes >= max) {
				max = _children[i]->_nodes;
				indices.push_back(i);
			}
		}

		// En cas d'égalité, on trie par évaluation
		int max_eval = -INT_MAX;
		int index = 0;

		for (int i = 0; i < indices.size(); i++) {
			if (_children[indices[i]]->_board._evaluation * color > max_eval) {
				max_eval = _children[indices[i]]->_board._evaluation * color;
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
	_board.reset_board();
	_move = Move();
	_new_node = true;

	for (int i = 0; i < children_count(); i++) {
		_children[i]->reset();
		delete _children[i];
	}

	_children.clear();
}

// Fonction qui renvoie les variantes d'exploration
string Node::get_exploration_variants(bool main) {

	// TODO: il faut rajouter les 1. .., 2...
	// Pour les eval, il faut afficher différemment les mats
	// Afficher les icônes des pièces (♔, ♕, ♖, ♗, ♘, ♙, ♚, ♛, ♜, ♝, ♞, ♟) (UTF-8)

	// Si on est en fin de variante
	if (_board._game_over_value) {
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
			int color = _board.get_color();

			children_evaluations.push_back(make_pair(- _children[children_nodes[0].second]->_board._evaluation * color, children_nodes[0].second));

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
						children_evaluations.push_back(make_pair(- _children[children_nodes[i].second]->_board._evaluation * color, children_nodes[i].second));
					}
				}
				else {
					children_evaluations.push_back(make_pair(- _children[children_nodes[i].second]->_board._evaluation * color, children_nodes[i].second));
				}

				
			}


			for (int i = 0; i < children_count(); i++) {
				variants += _board.move_label(_children[children_index[i]]->_move) + " " + _children[children_index[i]]->get_exploration_variants(false);

				// Nombre de noeuds...
				variants += "\nEval: " + int_to_round_string(_children[children_index[i]]->_board._evaluation) + " | Nodes: " + int_to_round_string(_children[children_index[i]]->_nodes) + "\n\n";
			}
		}

		// Sinon, on affiche seulement le coup le plus exploré
		else {
			// Affiche seulement le premier coup (le plus exploré, et en cas d'égalité, celui avec la meilleure évaluation)
			int index_best_move = get_most_explored_child_index();

			variants += _board.move_label(_children[index_best_move]->_move) + " " + _children[index_best_move]->get_exploration_variants(false);
		}
		
	}

	// S'il n'y a pas de coups explorés
	else {

		// On affiche l'évaluation du plateau en fin de variante
		variants = "(" + int_to_round_string(_board._evaluation) + ")";
	}
	
	return variants;
}