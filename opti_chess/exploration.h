#pragma once

#include "board.h"
#include "buffer.h"


// TODO:
// Au lieu d'avoir un plateau, stocker seulement l'indice du plateau dans le buffer?

// Noeud de l'arbre d'exploration
class Node {
public:

	// Variables

	// Plateau : FIXME -> indice du plateau dans le buffer?
	Board _board;

	// Coup joué pour arriver à ce plateau (FIXME: est-ce déjà stocké dans le plateau?)
	Move _move;

	// Indice du plateau dans le buffer
	int _index = -1;

	// Fils
	vector<Node*> _children;

	// Nouveau noeud?
	bool _new_node = true;

	// Pour accélérer la recherche du premier coup non exploré
	int _latest_first_move_explored = -1;

	// Nombre de noeuds
	int _nodes = 0;

	// Evaluations des enfants
	//vector<int> _children_evaluations; // FIXME: faut-il utiliser des vecteurs ou des tableaux dynamiques?
	//int *_children_evaluations;

	// Nombre de noueds des enfants
	//vector<int> _children_nodes;
	//int *_children_nodes;


	// A rajouter : évaluation?, nombre de noeuds?...

	// Constructeurs

	// Constructeur avec un plateau, un indice et un coup
	Node(Board board, int index, Move move);


	// Fonctions

	// Fonction qui ajoute un fils
	void add_child(Node* child);

	// Fonction qui renvoie le nombre de fils
	[[nodiscard]] int children_count() const;

	// Fonction qui renvoie l'indice du fils associé au coup s'il existe, -1 sinon (dans le vecteur _children)
	[[nodiscard]] int get_child_index(Move move) const;

	// Fonction qui renvoie l'indice du premier coup qui n'a pas encore été ajouté, -1 sinon
	[[nodiscard]] int get_first_unexplored_move_index();

	// Nouveau GrogrosZero
	void grogros_zero(Buffer buffer, Evaluator eval, float beta, float k_add, int nodes);

	// Fonction qui explore un nouveau coup
	void explore_new_move(Buffer buffer, Evaluator eval);

	// Fonction qui explore dans un plateau fils pseudo-aléatoire
	void explore_random_child(Buffer buffer, Evaluator eval, const float beta, const float k_add);

	// Fonction qui renvoie un noeud fils pseudo-aléatoire (en fonction des évaluations et du nombre de noeuds)
	[[nodiscard]] int pick_random_child_index(const float beta, const float k_add);

	// Fonction qui renvoie le fils le plus exploré
	[[nodiscard]] int get_most_explored_child_index(bool decide_by_eval = true);

	// Reset le noeud et ses enfants, et les supprime tous
	void reset();

	// Fonction qui renvoie les variantes d'exploration
	[[nodiscard]] string get_exploration_variants(bool main = true);

	// Fonctions à rajouter: destruction des fils et de soi...

	// Destructeur : TODO (supprimer tous les tableaux dynamiques...)
	// Il faudra free tous les plateaux du buffer
};