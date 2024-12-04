#pragma once

#include "board.h"
#include "buffer.h"


// TODO:
// Au lieu d'avoir un plateau, stocker seulement l'indice du plateau dans le buffer?


struct Evaluation {

	// Variables

	// Valeur de l'�valuation
	int _value;

	// Incertitude de l'�valuation
	float _uncertainty;

	// WDL
	WDL _wdl;

	// Score moyen
	float _avg_score;

	// Evalu�?
	bool _evaluated = false;


	// Copie de l'�valuation
	Evaluation& operator=(const Evaluation& other) {
		// Recopie les param�tres d'�valuation
		_value = other._value;
		_uncertainty = other._uncertainty;
		_wdl = other._wdl;
		_avg_score = other._avg_score;
		_evaluated = other._evaluated;

		return *this;
	}

	// Comparateur
	bool operator>(Evaluation& other) {
		if (!other._evaluated)
			return true;

		if (!_evaluated)
			return false;

		return _value > other._value;
	}

	bool operator<(Evaluation& other) {
		if (!other._evaluated)
			return true;

		if (!_evaluated)
			return false;

		return _value < other._value;
	}
};

// Noeud de l'arbre d'exploration
class Node {
public:

	// Variables

	// Plateau : FIXME -> indice du plateau dans le buffer?
	Board* _board;

	// Coup jou� pour arriver � ce plateau (FIXME: est-ce d�j� stock� dans le plateau?)
	//Move _move;

	// Fils avec leur coup associ�
	map<Move, Node*> _children;

	// Fils
	//vector<Node*> _children;

	// Pour acc�l�rer la recherche du premier coup non explor�
	int _latest_first_move_explored = -1;

	// Nombre de noeuds
	int _nodes = 0;
	//int _nodes = count_children_nodes() + 1;

	// Nombre d'explorations par l'algorithme de GrogrosZero
	int _iterations = 0;

	// Nombre de fois que l'algorithme a voulu explorer ce noeud
	int _chosen_iterations = 0;

	// TODO: il faut plusieurs types de noeuds: noeuds quiet (ceux recherch�s par GrogrosZero), noeuds de quiescence, noeuds de transposition...
	// On utilisera les research_nodes pour le temps de calcul de Grogros, l'affichage des fl�ches etc...

	// Temps de calcul
	clock_t _time_spent = 0;

	// Profondeur de la quiescence search
	//int _quiescence_depth = 0;

	// Est-ce que ce noeud a �t� explor� de fa�on compl�te?
	bool _fully_explored = false;

	// Reste t-il encore quelque chose � explorer?
	bool _can_explore = true;

	// Evaluation statique de la position
	Evaluation _static_evaluation;

	// Evaluation de la position apr�s r�flexion
	Evaluation _deep_evaluation;

	// Est-ce un noeud final?
	bool _is_terminal = false;

	// Noeud initialis�?
	bool _initialized = false;

	// A rajouter : �valuation?, nombre de noeuds?...

	// Constructeurs

	// Constructeur avec un plateau, un indice et un coup
	Node(Board* board);


	// Fonctions

	// Fonction qui ajoute un fils
	void add_child(Node* child, Move move);

	// Fonction qui renvoie le nombre de fils
	[[nodiscard]] size_t children_count() const;

	// Fonction qui renvoie l'indice du fils associ� au coup s'il existe, -1 sinon (dans le vecteur _children)
	//[[nodiscard]] int get_child_index(Move move) const;

	// Fonction qui renvoie l'indice du premier coup qui n'a pas encore �t� ajout�, -1 sinon
	//[[nodiscard]] int get_first_unexplored_move_index(bool fully_explored = false);

	// Fonction qui renvoie le premier coup qui n'a pas encore �t� ajout�
	[[nodiscard]] Move get_first_unexplored_move(bool fully_explored = false);

	// Initie le noeud en fonction de son plateau
	void init_node();

	// Nouveau GrogrosZero
	void grogros_zero(Buffer* buffer, Evaluator* eval, float beta, float k_add, int nodes, int quiescence_depth, Network* network = nullptr);

	// Fonction qui explore un nouveau coup
	void explore_new_move(Buffer* buffer, Evaluator* eval, int quiescence_depth, Network* network = nullptr);

	// Fonction qui explore dans un plateau fils pseudo-al�atoire
	void explore_random_child(Buffer* buffer, Evaluator* eval, const float beta, const float k_add, int quiescence_depth, Network* network = nullptr);

	// Fonction qui renvoie un noeud fils pseudo-al�atoire (en fonction des �valuations et du nombre de noeuds)
	[[nodiscard]] Move pick_random_child(const float beta, const float k_add);

	// Fait un softmax sur les evaluations
	// TODO *** rendre statique (et faire pareil pour bon nombre de fonctions...)
	void softmax(double* evaluations, float* avg_scores, int size, const float beta, const float k_add) const;

	// Fonction qui renvoie le fils le plus explor�
	[[nodiscard]] Move get_most_explored_child_move(bool decide_by_eval = true);

	// Reset le noeud et ses enfants, et les supprime tous
	void reset();

	// Fonction qui renvoie les variantes d'exploration
	[[nodiscard]] string get_exploration_variants(bool main = true, bool quiescence = false);

	// Fonction qui renvoie la profondeur de la variante principale
	[[nodiscard]] int get_main_depth();

	// Fonction qui renvoie le meilleur coup
	[[nodiscard]] Move get_best_move();

	// Fonction qui renvoie le fils le plus explor�
	[[nodiscard]] Node* get_most_explored_child(bool decide_by_eval = true);

	// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
	[[nodiscard]] int get_avg_nps() const;

	// Fonction qui renvoie le nombre d'it�rations par seconde
	[[nodiscard]] int get_ips() const;

	// Quiescence search int�gr� � l'exploration
	int quiescence(Buffer* buffer, Evaluator* eval, int depth, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr, bool use_custom_stand_pat = false, int stand_pat_value = 0);
	//void grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth);

	// Fonction qui renvoie le nombre de noeuds fils compl�tement explor�s
	[[nodiscard]] int get_fully_explored_children_count() const;

	// Fonction qui renvoie la somme des noeuds des fils
	int count_children_nodes() const;

	// Fonction qui renvoie le nombre de noeuds total
	int get_total_nodes() const;

	// Fonction qui �value la position
	void evaluate_position(Evaluator* eval, bool display = false, Network* network = nullptr, bool game_over_check = false);

	// Fonctions � rajouter: destruction des fils et de soi...

	// Destructeur : TODO (supprimer tous les tableaux dynamiques...)
	// Il faudra free tous les plateaux du buffer

	// Destructeur
	~Node();
};