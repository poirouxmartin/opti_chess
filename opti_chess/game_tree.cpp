#include "game_tree.h"
#include "board.h"
#include "gui.h"

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
string GameTreeNode::tree_display(GameTreeNode *current_node) {
	bool is_current_node = this == current_node;
	
	string display = is_current_node ? "()" : ""; // FIXME: à améliorer

	// Affichage des fils

	// Variation principale
	if (_children.size() > 0)
		display += + " " + (_board._player ? to_string(_board._moves_count) + ". " : "") + _children[0]._move_label;

	// Autres variations
	for (int i = 1; i < _children.size(); i++)
		display += " (" + to_string(_board._moves_count) + (_board._player ?  + ". " : "... ") + _children[i]._move_label + _children[i].tree_display(current_node) + ")";

	// Variation principale
	if (_children.size() > 0)
		display += _children[0].tree_display(current_node);

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

	if (can_go_forward) {
		main_GUI.play_move_keep((_current_node->_children[0])._move);
	}

	return can_go_forward;
}

// Sélection du noeud précédent
bool GameTree::select_previous_node() {
	bool can_go_back = _current_node != _root;

	if (can_go_back) {
		_current_node = _current_node->_parent;

		// Il faut aussi remonter le plateau pour l'exploration
		main_GUI._root_exploration_node->reset();
		main_GUI._root_exploration_node->_board = &_current_node->_board;
		main_GUI._board = _current_node->_board;
		main_GUI._board.reset_eval();
		main_GUI._board.update_bitboards();

		// Actualisation de l'affichage
		main_GUI._pgn = tree_display();
	}

	return can_go_back;
}

// Ajout d'un fils
void GameTree::add_child(GameTreeNode child) {
	_current_node->add_child(child);
}

// Ajout d'un fils à partir d'un plateau et d'un coup
void GameTree::add_child(Board board, Move move, string move_label) {

	// Vérifie que ce n'est pas un coup nul
	if (move.is_null_move()) {
		cout << "null move added, in position " << _current_node->_board.to_fen() << endl;
		return;
	}

	// Vérifie que le coup n'existe pas déjà
	for (int i = 0; i < _current_node->_children.size(); i++)
		if (_current_node->_children[i]._move == move)
			return;

	board.make_move(move, false, true);

	_current_node->add_child(GameTreeNode(board, move, move_label, *_current_node));
}

// Ajout d'un fils à partir d'un coup
void GameTree::add_child(Move move, string additional_label) {

	// Vérifie que ce n'est pas un coup nul
	if (move.is_null_move()) {
		cout << "null move added, in position " << _current_node->_board.to_fen() << endl;
		return;
	}

	// Vérifie que le coup n'existe pas déjà
	for (int i = 0; i < _current_node->_children.size(); i++)
		if (_current_node->_children[i]._move == move)
			return;

	Board board = _current_node->_board;
	string move_label = board.move_label(move) + (additional_label.empty() ? "" : " " + additional_label);
	board.make_move(move, false, true);

	_current_node->add_child(GameTreeNode(board, move, move_label, *_current_node));
}

// Affichage de l'arbre
string GameTree::tree_display() {
	return _root->tree_display(_current_node);
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

	// FIXME : y'a encore des bugs........

	// Si on est à la racine, on ne peut pas promouvoir
	if (_current_node == _root)
		return false;

	// On regarde d'abord si le coup actuel est le principal de la variante

	// Si oui, on regarde le parent
	if (_current_node->_parent->_children[0]._move == _current_node->_move) {
		_current_node = _current_node->_parent;

		// On promeut le parent
		return promote_current_variation();
	}

	// Si non, on promeut la variante en tant que variante principale

	// On stocke d'abord la variante actuelle
	GameTreeNode temp = *_current_node;

	// On décale les autres variantes jusqu'à la variante actuelle
	
	// On cherche la variante actuelle
	int variant_position = 0;
	for (int i = 1; i < _current_node->_parent->_children.size(); i++) {
		if (_current_node->_parent->_children[i]._move == _current_node->_move) {
			variant_position = i;
			break;
		}
	}
		
	// On décale les variantes
	for (int j = variant_position; j > 0; j--)
		_current_node->_parent->_children[j] = _current_node->_parent->_children[j - 1];

	// On place la variante actuelle en première position
	_current_node->_parent->_children[0] = temp;

	//*_current_node = _current_node->_parent->_children[0];
	// FIXME: ça marche pas, on est pas sur le bon noeud

	return true;
}