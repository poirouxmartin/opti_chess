#pragma once
#include "board.h"
#include <vector>


// Noeud de l'arbre de jeu
class GameTreeNode {
	public:

	// Dernier coup joué
	Move _move;

	// Label du coup
	string _move_label;

	// Plateau
	Board _board;

	// Fils
	vector<GameTreeNode> _children;

	// Parent
	GameTreeNode* _parent;

	// Constructeur par défaut
	GameTreeNode();

	// Constructeur à partir d'un plateau et d'un coup
	GameTreeNode(Board, Move, string, const GameTreeNode&);

	// Méthodes

	// Ajout d'un fils
	void add_child(GameTreeNode);

	// Affichage de l'arbre
	string tree_display();

	// Reset (ne pas oublier de vider la mémoire)
	void reset();
};


// Arbre de jeu (pour stocker les variations principales dans la GUI)
class GameTree {
	public:

	// Racine
	GameTreeNode* _root;

	// Noeud actuel
	GameTreeNode* _current_node;


	// Constructeur par défaut
	GameTree();

	// Constructeur à partir d'un plateau
	GameTree(Board);

	// Méthodes

	// Sélection du noeud suivant
	bool select_next_node(Move move);

	// Sélection du premier noeud suivant
	bool select_first_next_node();

	// Sélection du noeud précédent
	bool select_previous_node();

	// Ajout d'un fils
	void add_child(GameTreeNode);

	// Ajout d'un fils à partir d'un plateau et d'un coup
	void add_child(Board, Move, string);

	// Affichage de l'arbre
	string tree_display();

	// Reset
	void reset();

	// Nouvel arbre à partir d'un plateau
	void new_tree(Board&);
};
