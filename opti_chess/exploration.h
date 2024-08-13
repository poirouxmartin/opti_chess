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

	// Coup joué pour arriver à ce plateau (FIXME: est-ce déjà stocké dans le plateau?)
	//Move _move;

	// Fils avec leur coup associé
	map<Move, Node*> _children;

	// Fils
	//vector<Node*> _children;

	// Nouveau noeud?
	bool _new_node = true;

	// Pour accélérer la recherche du premier coup non exploré
	int _latest_first_move_explored = -1;

	// Nombre de noeuds
	int _nodes = 0;
	//int _nodes = count_children_nodes() + 1;

	// Nombre d'itérations par l'algorithme de GrogrosZero (TODO)
	int _iterations = 0;

	// TODO: il faut plusieurs types de noeuds: noeuds quiet (ceux recherchés par GrogrosZero), noeuds de quiescence, noeuds de transposition...
	// On utilisera les research_nodes pour le temps de calcul de Grogros, l'affichage des flèches etc...

	// Temps de calcul
	clock_t _time_spent = 0;

	// Profondeur de la quiescence search
	//int _quiescence_depth = 0;

	// Est-ce que ce noeud a été exploré de façon complète?
	bool _fully_explored = false;

	// A rajouter : évaluation?, nombre de noeuds?...

	// Constructeurs

	// Constructeur avec un plateau, un indice et un coup
	Node(Board* board);


	// Fonctions

	// Fonction qui ajoute un fils
	void add_child(Node* child, Move move);

	// Fonction qui renvoie le nombre de fils
	[[nodiscard]] int children_count() const;

	// Fonction qui renvoie l'indice du fils associé au coup s'il existe, -1 sinon (dans le vecteur _children)
	//[[nodiscard]] int get_child_index(Move move) const;

	// Fonction qui renvoie l'indice du premier coup qui n'a pas encore été ajouté, -1 sinon
	//[[nodiscard]] int get_first_unexplored_move_index(bool fully_explored = false);

	// Fonction qui renvoie le premier coup qui n'a pas encore été ajouté
	[[nodiscard]] Move get_first_unexplored_move(bool fully_explored = false);

	// Nouveau GrogrosZero
	void grogros_zero(Buffer* buffer, Evaluator* eval, float beta, float k_add, int nodes, int quiescence_depth, Network* network = nullptr);

	// Fonction qui explore un nouveau coup
	void explore_new_move(Buffer* buffer, Evaluator* eval, int quiescence_depth, Network* network = nullptr);

	// Fonction qui explore dans un plateau fils pseudo-aléatoire
	void explore_random_child(Buffer* buffer, Evaluator* eval, const float beta, const float k_add, int quiescence_depth, Network* network = nullptr);

	// Fonction qui renvoie un noeud fils pseudo-aléatoire (en fonction des évaluations et du nombre de noeuds)
	[[nodiscard]] Move pick_random_child(const float beta, const float k_add);

	// Fonction qui renvoie le ratio de victoire pour les blancs du noeud (le ratio de victoire pour une eval alpha est de beta)
	[[nodiscard]] float win_ratio(double alpha = 100.0, double beta = 0.67) const;

	// Fonction qui calcule le score UCT
	[[nodiscard]] float uct_score(int total_nodes, float alpha) const;

	// Fonction qui renvoie le coup avec le meilleur score UCT
	[[nodiscard]] Move pick_best_uct_child(float alpha = 3.0f);

	// Fonction qui renvoie le fils le plus exploré
	[[nodiscard]] Move get_most_explored_child_move(bool decide_by_eval = true);

	// Reset le noeud et ses enfants, et les supprime tous
	void reset();

	// Fonction qui renvoie les variantes d'exploration
	[[nodiscard]] string get_exploration_variants(bool main = true);

	// Fonction qui renvoie la profondeur de la variante principale
	[[nodiscard]] int get_main_depth();

	// Fonction qui renvoie le meilleur coup
	[[nodiscard]] Move get_best_move();

	// Fonction qui renvoie le fils le plus exploré
	[[nodiscard]] Node* get_most_explored_child(bool decide_by_eval = true);

	// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
	[[nodiscard]] int get_avg_nps() const;

	// Quiescence search intégré à l'exploration
	int grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr);
	//void grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth);

	// Fonction qui renvoie le nombre de noeuds fils complètement explorés
	[[nodiscard]] int get_fully_explored_children_count() const;

	// Tentative de quiescence maison
	void new_grogros_quiescence(Buffer* buffer, Evaluator* eval, float beta, float k_add, int nodes);

	// Fonction qui renvoie la somme des noeuds des fils
	int count_children_nodes() const;

	// Fonction qui renvoie le nombre de noeuds total
	int get_total_nodes() const;

	// Fonctions à rajouter: destruction des fils et de soi...

	// Destructeur : TODO (supprimer tous les tableaux dynamiques...)
	// Il faudra free tous les plateaux du buffer

	// Destructeur
	~Node();
};