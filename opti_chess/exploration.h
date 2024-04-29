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
	Board* _board;

	// Coup jou� pour arriver � ce plateau (FIXME: est-ce d�j� stock� dans le plateau?)
	Move _move;

	// Fils
	vector<Node*> _children;

	// Nouveau noeud?
	bool _new_node = true;

	// Pour acc�l�rer la recherche du premier coup non explor�
	int _latest_first_move_explored = -1;

	// Nombre de noeuds
	int _nodes = 0;

	// Temps de calcul
	clock_t _time_spent = 0;

	// A rajouter : �valuation?, nombre de noeuds?...

	// Constructeurs

	// Constructeur avec un plateau, un indice et un coup
	Node(Board* board, Move move);


	// Fonctions

	// Fonction qui ajoute un fils
	void add_child(Node* child);

	// Fonction qui renvoie le nombre de fils
	[[nodiscard]] int children_count() const;

	// Fonction qui renvoie l'indice du fils associ� au coup s'il existe, -1 sinon (dans le vecteur _children)
	[[nodiscard]] int get_child_index(Move move) const;

	// Fonction qui renvoie l'indice du premier coup qui n'a pas encore �t� ajout�, -1 sinon
	[[nodiscard]] int get_first_unexplored_move_index();

	// Nouveau GrogrosZero
	void grogros_zero(Buffer* buffer, Evaluator* eval, float beta, float k_add, int nodes, int quiescence_depth);

	// Fonction qui explore un nouveau coup
	void explore_new_move(Buffer* buffer, Evaluator* eval, int quiescence_depth);

	// Fonction qui explore dans un plateau fils pseudo-al�atoire
	void explore_random_child(Buffer* buffer, Evaluator* eval, const float beta, const float k_add, int quiescence_depth);

	// Fonction qui renvoie un noeud fils pseudo-al�atoire (en fonction des �valuations et du nombre de noeuds)
	[[nodiscard]] int pick_random_child_index(const float beta, const float k_add);

	// Fonction qui renvoie le fils le plus explor�
	[[nodiscard]] int get_most_explored_child_index(bool decide_by_eval = true);

	// Reset le noeud et ses enfants, et les supprime tous
	void reset();

	// Fonction qui renvoie les variantes d'exploration
	[[nodiscard]] string get_exploration_variants(bool main = true);

	// Fonction qui renvoie la profondeur de la variante principale
	[[nodiscard]] int get_main_depth();

	// Fonction qui renvoie le meilleur coup
	[[nodiscard]] Move get_best_move();

	// Fonction qui renvoie le fils le plus explor�
	[[nodiscard]] Node* get_most_explored_child(bool decide_by_eval = true);

	// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
	[[nodiscard]] int get_avg_nps() const;

	// Quiescence search int�gr� � l'exploration
	int grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth, int alpha = -INT_MAX, int beta = INT_MAX);
	//void grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth);

	// Fonctions � rajouter: destruction des fils et de soi...

	// Destructeur : TODO (supprimer tous les tableaux dynamiques...)
	// Il faudra free tous les plateaux du buffer

	// Destructeur
	~Node();
};