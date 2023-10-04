#include "game_tree.h"
#include "board.h"

// Constructeur par défaut
GameTreeNode::GameTreeNode() {
}

// Constructeur à partir d'un plateau et d'un coup
GameTreeNode::GameTreeNode(Board board, Move move, string move_label, const GameTreeNode& parent) {
	_move = move;
	_move_label = move_label;
	_board = board;
	_parent = (GameTreeNode*)&parent;
}

// Ajout d'un fils
void GameTreeNode::add_child(GameTreeNode child) {
	_children.push_back(child);
}

// Affichage de l'arbre
string GameTreeNode::tree_display() {
	string display = "";

	// Affichage des fils

	// Variation principale
	if (_children.size() > 0)
		display += + " " + (_board._player ? to_string(_board._moves_count) + ". " : "") + _children[0]._move_label;

	// Autres variations
	for (int i = 1; i < _children.size(); i++)
		display += " (" + to_string(_board._moves_count) + (_board._player ?  + ". " : "... ") + _children[i]._move_label + _children[i].tree_display() + ")";

	// Variation principale
	if (_children.size() > 0)
		display += _children[0].tree_display();

	return display;
}

// Reset (ne pas oublier de vider la mémoire)
void GameTreeNode::reset() {
	_move = Move();
	_move_label = "";
	_board = Board();
	_children.clear();
}


// Constructeur par défaut
GameTree::GameTree() {
}

// Constructeur à partir d'un plateau
GameTree::GameTree(Board board) {
	_root = new GameTreeNode(board, Move(), "", GameTreeNode());
	_current_node = _root;
}

// Sélection du noeud suivant
bool GameTree::select_next_node(Move move) {
	for (int i = 0; i < _current_node->_children.size(); i++)
		if (_current_node->_children[i]._move == move) {
			_current_node = &(_current_node->_children[i]);
			return true;
		}

	return false;
}

// Sélection du premier noeud suivant
bool GameTree::select_first_next_node() {
	bool can_go_forward = _current_node->_children.size() > 0;

	if (can_go_forward)
		_current_node = &(_current_node->_children[0]);

	return can_go_forward;
}

// Sélection du noeud précédent
bool GameTree::select_previous_node() {
	bool can_go_back = _current_node != _root;

	if (can_go_back)
		_current_node = _current_node->_parent;

	return can_go_back;
}

// Ajout d'un fils
void GameTree::add_child(GameTreeNode child) {
	_current_node->add_child(child);
}

// Ajout d'un fils à partir d'un plateau et d'un coup
void GameTree::add_child(Board board, Move move, string move_label) {
	// Vérifie que le coup n'existe pas déjà
	for (int i = 0; i < _current_node->_children.size(); i++)
		if (_current_node->_children[i]._move == move)
			return;

	board.make_move(move);

	_current_node->add_child(GameTreeNode(board, move, move_label, *_current_node));
}

// Affichage de l'arbre
string GameTree::tree_display() {
	return _root->tree_display();
}

// Reset
void GameTree::reset() {
	_root->reset();
	_current_node = _root;
}

// Nouvel arbre à partir d'un plateau
void GameTree::new_tree(Board& board) {
	_root->reset();
	_root = new GameTreeNode(board, Move(), "", GameTreeNode());
	_current_node = _root;
}

// Promeut la variante actuelle en tant que variante principale
bool GameTree::promote_current_variation() {
	// Vérifie que le noeud actuel a un parent
	if (_current_node == _root)
		return false;
	
	// TODO : il faut promouvoir la branche entière, pas seulement le noeud actuel

	// Cherche la variante actuelle dans les fils du parent
	for (int i = 0; i < _current_node->_parent->_children.size(); i++)
		if (_current_node->_parent->_children[i]._move == _current_node->_move) {
			if (i == 0)
				return false;
				
			// Place la variation en première position
			GameTreeNode temp = _current_node->_parent->_children[0];
			_current_node->_parent->_children[0] = _current_node->_parent->_children[i];
			// FIXME : ici ça swap, au lieu de simplement mettre la variante en première position, et de décaler les autres
			_current_node->_parent->_children[i] = temp;

			return true;
		}
}