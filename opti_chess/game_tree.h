#pragma once
#include "board.h"
#include <vector>
#include <memory>


// Node of the game tree
class GameTreeNode {
	public:

	// Last move played
	Move _move;

	// Label of the move
	string _move_label;

	// Board
	Board _board;

	// Children (heap nodes: addresses stay valid when siblings are added or reordered,
	// _parent / _current_node / the GUI board pointer all point into the tree)
	vector<unique_ptr<GameTreeNode>> _children;

	// Parent (nullptr at the root)
	GameTreeNode* _parent = nullptr;

	// Time left for the player who is not to move
	clock_t _time; // TODO

	// TODO: (to add to the PGN)
	// Node count, evaluation, special move notation...

	// Default constructor
	GameTreeNode();

	// Constructor from a board, a move and the parent node
	GameTreeNode(Board, Move, string, GameTreeNode* parent);

	// Methods

	// Adds a child
	void add_child(unique_ptr<GameTreeNode>);

	// Displays the tree
	string tree_display(GameTreeNode *current_node);

	// Reset (do not forget to free the memory)
	void reset();
};


// Game tree (stores the main variations for the GUI)
class GameTree {
	public:

	// Root
	GameTreeNode* _root;

	// Current node
	GameTreeNode* _current_node;


	// Default constructor
	GameTree();

	// Constructor from a board
	GameTree(Board);

	// Methods

	// Selects the next node
	bool select_next_node(Move move);

	// Selects the first following node
	bool select_first_next_node();

	// Selects the previous node
	bool select_previous_node();

	// Adds a child from a board and a move
	void add_child(Board, Move, string);

	// Adds a child from a move
	void add_child(Move move, string additionnal_label);

	// Displays the tree
	string tree_display();

	// Reset
	void reset();

	// New tree from a board
	void new_tree(Board&);

	// Promotes the current variation to the main variation
	bool promote_current_variation();
};
