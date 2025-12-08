#pragma once

#include "board.h"
#include "buffer.h"
#include <robin_map.h>

using namespace tsl;

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
	robin_map<Move, Node*> _children;

	// Fils
	//vector<Node*> _children;

	// Pour accélérer la recherche du premier coup non exploré
	int _latest_first_move_explored = -1;

	// Nombre de noeuds
	int _nodes = 0;
	//int _nodes = count_children_nodes() + 1;

	// Nombre d'explorations par l'algorithme de GrogrosZero
	int _iterations = 0;

	// Nombre de fois que l'algorithme a voulu explorer ce noeud
	int _chosen_iterations = 0;

	// TODO: il faut plusieurs types de noeuds: noeuds quiet (ceux recherchés par GrogrosZero), noeuds de quiescence, noeuds de transposition...
	// On utilisera les research_nodes pour le temps de calcul de Grogros, l'affichage des flèches etc...

	// Temps de calcul
	clock_t _time_spent = 0;

	// Profondeur de la quiescence search
	int _quiescence_depth = 0;

	// Est-ce que ce noeud a été exploré de façon complète?
	bool _fully_explored = false;

	// Reste t-il encore quelque chose à explorer?
	bool _can_explore = true;

	// Evaluation statique de la position
	Evaluation _static_evaluation;

	// Evaluation de la position après réflexion
	Evaluation _deep_evaluation;

	// Est-ce un noeud final?
	bool _is_terminal = false;

	// Noeud initialisé?
	bool _initialized = false;

	// La valeur d'évaluation est le standpat
	bool _is_stand_pat_eval = true;

	// A rajouter : évaluation?, nombre de noeuds?...

	// Constructeurs

	// Constructeur avec un plateau, un indice et un coup
	Node(Board* board);


	// Fonctions

	// Fonction qui ajoute un fils
	void add_child(Node* child, Move move);

	// Fonction qui renvoie le nombre de fils
	size_t children_count() const;

	// Fonction qui renvoie l'indice du fils associé au coup s'il existe, -1 sinon (dans le vecteur _children)
	//int get_child_index(Move move) const;

	// Fonction qui renvoie l'indice du premier coup qui n'a pas encore été ajouté, -1 sinon
	//int get_first_unexplored_move_index(bool fully_explored = false);

	// Fonction qui renvoie le premier coup qui n'a pas encore été ajouté
	Move get_first_unexplored_move(bool fully_explored = false);

	// Initie le noeud en fonction de son plateau
	void init_node();

	// Nouveau GrogrosZero
	void grogros_zero(BoardBuffer* buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int nodes, int quiescence_depth, Network* network = nullptr);

	// Fonction qui explore un nouveau coup
	void explore_new_move(BoardBuffer* buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr);

	// Fonction qui explore dans un plateau fils pseudo-aléatoire
	void explore_random_child(BoardBuffer* buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr);

	// Fonction qui renvoie le fils le plus exploré
	Move get_most_explored_child_move(bool decide_by_eval = false);

	// Reset le noeud et ses enfants, et les supprime tous
	void reset();

	// Fonction qui renvoie les variantes d'exploration
	string get_exploration_variants(const double alpha, const double beta, bool main = true, bool quiescence = false);

	// Fonction qui renvoie la profondeur de la variante principale
	int get_main_depth(const double alpha, const double beta);

	// Fonction qui renvoie le fils le plus exploré
	Node* get_most_explored_child(bool decide_by_eval = true);

	// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
	int get_avg_nps() const;

	// Fonction qui renvoie le nombre d'itérations par seconde
	int get_ips() const;

	// Quiescence search intégré à l'exploration
	int quiescence(BoardBuffer* buffer, Evaluator* evaluator, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr, bool evaluate_threats = true, int beta_margin = 0);
	//void grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth);

	// Fonction qui renvoie le nombre de noeuds fils complètement explorés
	int get_fully_explored_children_count() const;

	// Fonction qui renvoie la somme des noeuds des fils
	int count_children_nodes() const;

	// Fonction qui renvoie le nombre de noeuds total
	int get_total_nodes() const;

	// Fonction qui évalue la position
	void evaluate_position(Evaluator* evaluator, bool display = false, Network* network = nullptr, bool game_over_check = true);

	// Fonction qui renvoie un noeud fils pseudo-aléatoire (en fonction des évaluations et du nombre de noeuds)
	Move pick_random_child(const double alpha, const double beta, const double gamma);

	// Fonction qui renvoie le score d'un coup. Alpha augmente l'importance de l'évaluation, et beta augmente l'importance du winrate
	robin_map<Move, double> get_move_scores(const double alpha, const double beta, const bool consider_standpat = false, const int qdepth = -100);

	// Fonction qui renvoie la valeur du noeud
	double get_node_score(const double alpha, const double beta, const int max_eval, const double max_avg_score, const bool player, Evaluation *custom_eval = nullptr) const;

	// Fonction qui renvoie le coup avec le meilleur score
	Move get_best_score_move(const double alpha, const double beta, const bool consider_standpat = false, const int qdepth = -100);

	// Fonction qui renvoie une valeur prévisionnelle du score du noeud, lorsqu'on ne connait pas les évaluations max (pour la quiecence)
	int get_previsonal_node_score(const double alpha, const double beta, const bool player) const;

	// Fonction qui évalue la menace en utilisant une quiesence sur le tour de l'adversaire
	int evaluate_quiescence_threat(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr) const;

	// Quiescence minimale (sans stockage des noeuds)
	int minimal_quiescence(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr);

	// Fonctions à rajouter: destruction des fils et de soi...

	// Destructeur : TODO (supprimer tous les tableaux dynamiques...)
	// Il faudra free tous les plateaux du buffer

	// Destructeur
	~Node();
};