#include "exploration.h"
#include "useful_functions.h"

// Constructeur avec un plateau, un indice et un coup
Node::Node(Board board, int index, Move move)
{
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
[[nodiscard]] int Node::get_child(Move move) const {
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
		if (get_child(_board._moves[i]) == -1) {
			_latest_first_move_explored = i;
			return i;
		}
	}

	return -1;
}

// Nouveau GrogrosZero
void Node::grogros_zero(Buffer buffer, Evaluator eval, float beta, float k_add, int nodes)
{
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
void Node::explore_new_move(Buffer buffer, Evaluator eval)
{
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
void Node::explore_random_child(Buffer buffer, Evaluator eval, float beta, float k_add)
{
	// Prend un fils aléatoire
	int child_index = pick_random_child(beta, k_add);

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
int Node::pick_random_child(const float beta, const float k_add)
{
	bool color = _board._player;
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