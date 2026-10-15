#include "game_tree.h"
#include "board.h"
#include "gui.h"
#include "zobrist.h"
#include <algorithm>

// Default constructor
GameTreeNode::GameTreeNode() {
}

// Constructor from a board and a move
GameTreeNode::GameTreeNode(Board board, Move move, string move_label, GameTreeNode* parent) {
	_move = move;
	_move_label = move_label;
	_board = board;
	_parent = parent;
}

// Adds a child
void GameTreeNode::add_child(unique_ptr<GameTreeNode> child) {
	child->_parent = this;
	_children.push_back(std::move(child));
}

// Displays the tree
string GameTreeNode::tree_display(GameTreeNode *current_node) {
	bool is_current_node = this == current_node;
	
	string display = is_current_node ? "()" : ""; // FIXME: to be improved

	// Display of the children

	// Principal variation
	if (_children.size() > 0)
		display += + " " + (_board._player ? to_string(_board._moves_count) + ". " : "") + _children[0]->_move_label;

	// Other variations
	for (int i = 1; i < _children.size(); i++)
		display += " (" + to_string(_board._moves_count) + (_board._player ?  + ". " : "... ") + _children[i]->_move_label + _children[i]->tree_display(current_node) + ")";

	// Principal variation
	if (_children.size() > 0)
		display += _children[0]->tree_display(current_node);

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
	_root = new GameTreeNode(board, Move(), "", nullptr);
	_current_node = _root;
}

// Selects the next node
bool GameTree::select_next_node(Move move) {
	for (int i = 0; i < _current_node->_children.size(); i++)
		if (_current_node->_children[i]->_move == move) {
			_current_node = _current_node->_children[i].get();
			return true;
		}

	return false;
}

// Selects the first following node
bool GameTree::select_first_next_node() {
	bool can_go_forward = _current_node->_children.size() > 0;

	if (can_go_forward) {
		main_GUI.play_move_keep(_current_node->_children[0]->_move);
	}

	return can_go_forward;
}

// Selects the previous node
bool GameTree::select_previous_node() {
	bool can_go_back = _current_node != _root;

	if (can_go_back) {
		main_GUI.stop_compute(); // worker first: root reset + board swap below
		main_GUI._position_epoch++; // real tree mutation: worker drops hint maps on next wake
		std::lock_guard<std::mutex> tree_lk(main_GUI._tree_mutex);
		_current_node = _current_node->_parent;
		// Back before the terminal position: the game is playable again.
		main_game_over = false;
		// Force a fresh terminal evaluation (threefold): the adopted board
		// may carry a stale search-time memo (twofold), which would
		// re-trigger the game-over branch without rechecking.
		main_GUI._board->_game_over_checked = false;

		// The board must be moved back up for the exploration too
		main_GUI.reset_buffers(); // #6: systematic TT/node_map clear
		main_GUI._root_exploration_node->reset();
		main_GUI._root_exploration_node->_board = &_current_node->_board;
		main_GUI._root_exploration_node->_is_active = true;
		main_GUI._board = &_current_node->_board;
		main_GUI._board->reset_eval();
		main_GUI._board->update_bitboards();
		main_GUI._tree_snapshot.valid = false;
		main_GUI._tree_snapshot.arrows.clear();

		// Refresh the display
		main_GUI._pgn = tree_display();
		main_GUI.log_position("back");
	}

	return can_go_back;
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
		if (_current_node->_children[i]->_move == move)
			return;

	board.make_move(move, false, true);

	_current_node->add_child(make_unique<GameTreeNode>(board, move, move_label, _current_node));
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
		if (_current_node->_children[i]->_move == move)
			return;

	Board board = _current_node->_board;
	string move_label = board.move_label(move) + (additional_label.empty() ? "" : " " + additional_label);
	board.make_move(move, false, true);

	_current_node->add_child(make_unique<GameTreeNode>(board, move, move_label, _current_node));
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
	_root = new GameTreeNode(board, Move(), "", nullptr);
	_current_node = _root;
}

// Promotes the current variation to the main variation
bool GameTree::promote_current_variation() {

	// Walk up to the first ancestor level where the line is not the main one, and move it
	// to first place there. Nodes are heap-allocated, so only the owning pointers move:
	// _current_node, _parent links and the GUI board pointer all stay valid.
	for (GameTreeNode* node = _current_node; node != _root; node = node->_parent) {
		auto& siblings = node->_parent->_children;
		int position = 0;
		while (position < (int)siblings.size() && siblings[position].get() != node)
			position++;
		if (position == (int)siblings.size())
			return false; // not linked to its parent: corrupt tree, leave it alone
		if (position == 0)
			continue;

		std::rotate(siblings.begin(), siblings.begin() + position, siblings.begin() + position + 1);
		return true;
	}

	// Already on the main line (or at the root)
	return false;
}