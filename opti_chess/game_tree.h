#pragma once
#include "opti_chess.h"
#include <vector>


// Noeud de l'arbre de jeu
class GameTreeNode {

	// Dernier coup jou�
	Move _move;

	// Plateau
	Board _board;

	// Fils
	vector<GameTreeNode> _children;


	// Constructeur par d�faut
	GameTreeNode();

	// Constructeur � partir d'un plateau et d'un coup
	GameTreeNode(const Board&, const Move&);

	// M�thodes

	// Ajout d'un fils
	void add_child(GameTreeNode);

	// Affichage de l'arbre
	string tree_display();
};


// Arbre de jeu (pour stocker les variations principales dans la GUI)
class GameTree {

	// Racine
	GameTreeNode _root;

	// Noeud actuel
	GameTreeNode* _current_node;


	// Constructeur par d�faut
	GameTree();

	// M�thodes

	// S�lection du noeud suivant
	void select_next_node();

	// S�lection du noeud pr�c�dent
	void select_previous_node();

};
