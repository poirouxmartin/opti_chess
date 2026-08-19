#include "game_tree.h"
#include "board.h"
#include "gui.h"
#include "zobrist.h"

// Default constructor
GameTreeNode::GameTreeNode() {
}

// Constructor from a board and a move
GameTreeNode::GameTreeNode(Board board, Move move, string move_label, const GameTreeNode& parent) {
	_move = move;
	_move_label = move_label;
	_board = board;
	_parent = (GameTreeNode*)&parent;
}

// Adds a child
void GameTreeNode::add_child(GameTreeNode child) {
	_children.push_back(child);
}

// Displays the tree
string GameTreeNode::tree_display(GameTreeNode *current_node) {
	bool is_current_node = this == current_node;
	
	string display = is_current_node ? "()" : ""; // FIXME: to be improved

	// Display of the children

	// Principal variation
	if (_children.size() > 0)
		display += + " " + (_board._player ? to_string(_board._moves_count) + ". " : "") + _children[0]._move_label;

	// Other variations
	for (int i = 1; i < _children.size(); i++)
		display += " (" + to_string(_board._moves_count) + (_board._player ?  + ". " : "... ") + _children[i]._move_label + _children[i].tree_display(current_node) + ")";

	// Principal variation
	if (_children.size() > 0)
		display += _children[0].tree_display(current_node);

	return display;
}

// Reset (do not forget to free the memory)
void GameTreeNode::reset() {
	_move = Move();
	_move_label = "";
	_board = Board();
	_children.clear();
}


// Default constructor
GameTree::GameTree() : _root(nullptr), _current_node(nullptr) {
}

// Constructor from a board
GameTree::GameTree(Board board) {
	_root = new GameTreeNode(board, Move(), "", GameTreeNode());
	_current_node = _root;
}

// Selects the next node
bool GameTree::select_next_node(Move move) {
	for (int i = 0; i < _current_node->_children.size(); i++)
		if (_current_node->_children[i]._move == move) {
			_current_node = &(_current_node->_children[i]);
			return true;
		}

	return false;
}

// Selects the first following node
bool GameTree::select_first_next_node() {
	bool can_go_forward = _current_node->_children.size() > 0;

	if (can_go_forward) {
		main_GUI.play_move_keep((_current_node->_children[0])._move);
	}

	return can_go_forward;
}

// Selects the previous node
bool GameTree::select_previous_node() {
	bool can_go_back = _current_node != _root;

	if (can_go_back) {
		_current_node = _current_node->_parent;

		// The board must be moved back up for the exploration too
		main_GUI.reset_buffers(); // #6: systematic TT/node_map clear
		main_GUI._root_exploration_node->reset();
		main_GUI._root_exploration_node->_board = &_current_node->_board;
		main_GUI._root_exploration_node->_is_active = true;
		main_GUI._board = &_current_node->_board;
		main_GUI._board->reset_eval();
		main_GUI._board->update_bitboards();

		// Refresh the display
		main_GUI._pgn = tree_display();
	}

	return can_go_back;
}

// Adds a child
void GameTree::add_child(GameTreeNode child) {
	_current_node->add_child(child);
}

// Adds a child from a board and a move
void GameTree::add_child(Board board, Move move, string move_label) {

	// Check that this is not a null move
	if (move.is_null_move()) {
		cout << "null move added, in position " << _current_node->_board.to_fen() << endl;
		return;
	}

	// Check that the move does not already exist
	for (int i = 0; i < _current_node->_children.size(); i++)
		if (_current_node->_children[i]._move == move)
			return;

	board.make_move(move, false, true);

	_current_node->add_child(GameTreeNode(board, move, move_label, *_current_node));
}

// Adds a child from a move
void GameTree::add_child(Move move, string additional_label) {

	// Check that this is not a null move
	if (move.is_null_move()) {
		cout << "null move added, in position " << _current_node->_board.to_fen() << endl;
		return;
	}

	// Check that the move does not already exist
	for (int i = 0; i < _current_node->_children.size(); i++)
		if (_current_node->_children[i]._move == move)
			return;

	Board board = _current_node->_board;
	string move_label = board.move_label(move) + (additional_label.empty() ? "" : " " + additional_label);
	board.make_move(move, false, true);

	_current_node->add_child(GameTreeNode(board, move, move_label, *_current_node));
}

// Displays the tree
string GameTree::tree_display() {
	return _root->tree_display(_current_node);
}

// Reset
void GameTree::reset() {
	if (_root) {
		_root->reset();
		_current_node = _root;
	}
}

// New tree from a board
void GameTree::new_tree(Board& board) {
	if (_root)
		_root->reset();
	_root = new GameTreeNode(board, Move(), "", GameTreeNode());
	_current_node = _root;
}

// Promotes the current variation to the main variation
bool GameTree::promote_current_variation() {

	// FIXME: there are still bugs here........

	// At the root there is nothing to promote
	if (_current_node == _root)
		return false;

	// First check whether the current move is the main one of the variation

	// If it is, look at the parent
	if (_current_node->_parent->_children[0]._move == _current_node->_move) {
		_current_node = _current_node->_parent;

		// Promote the parent
		return promote_current_variation();
	}

	// Otherwise, promote the variation to the main variation

	// First store the current variation
	GameTreeNode temp = *_current_node;

	// Shift the other variations up to the current one
	
	// Look for the current variation
	int variant_position = 0;
	for (int i = 1; i < _current_node->_parent->_children.size(); i++) {
		if (_current_node->_parent->_children[i]._move == _current_node->_move) {
			variant_position = i;
			break;
		}
	}
		
	// Shift the variations
	for (int j = variant_position; j > 0; j--)
		_current_node->_parent->_children[j] = _current_node->_parent->_children[j - 1];

	// Put the current variation in first position
	_current_node->_parent->_children[0] = temp;

	//*_current_node = _current_node->_parent->_children[0];
	// FIXME: this does not work, we are not on the right node

	return true;
}