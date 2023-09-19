#pragma once
#include "opti_chess.h"
#include <vector>


// Noeud de l'arbre de jeu
class GameTreeNode {

	// Dernier coup joué
	Move _move;

	// Plateau
	Board _board;

	// Fils
	vector<GameTreeNode> _children;


	// Constructeur par défaut
	GameTreeNode();

	// Constructeur à partir d'un plateau et d'un coup
	GameTreeNode(const Board&, const Move&);

	// Méthodes

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


	// Constructeur par défaut
	GameTree();

	// Méthodes

	// Sélection du noeud suivant
	void select_next_node();

	// Sélection du noeud précédent
	void select_previous_node();

};
