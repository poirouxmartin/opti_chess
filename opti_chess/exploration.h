#pragma once

#include "board.h"
#include "buffer.h"
#include <robin_map.h>

using namespace tsl;

// TODO:
// Au lieu d'avoir un plateau, stocker seulement l'indice du plateau dans le buffer?


struct Evaluation {

	// Variables

	// Valeur de l'�valuation
	int _value;

	// Incertitude de l'�valuation
	float _uncertainty;

	// Valeur winnable
	float _winnable_white;
	float _winnable_black;

	// WDL
	WDL _wdl;

	// Score moyen
	float _avg_score;

	// TODO *** add total value (-> move score?)

	// Evalu�?
	bool _evaluated = false;


	// Copie de l'�valuation
	Evaluation& operator=(const Evaluation& other) {
		// Recopie les param�tres d'�valuation
		_value = other._value;
		_uncertainty = other._uncertainty;
		_winnable_white = other._winnable_white;
		_winnable_black = other._winnable_black;
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

	// Reset
	void reset() {
		_value = 0;
		_uncertainty = 0.0f;
		_winnable_white = 1.0f;
		_winnable_black = 1.0f;
		_wdl = WDL();
		_avg_score = 0.0f;
		_evaluated = false;
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
	int _quiescence_depth = 0;

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

	// La valeur d'�valuation est le standpat
	bool _is_stand_pat_eval = true;

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
	void grogros_zero(Buffer* buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int nodes, int quiescence_depth, Network* network = nullptr);

	// Fonction qui explore un nouveau coup
	void explore_new_move(Buffer* buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr);

	// Fonction qui explore dans un plateau fils pseudo-al�atoire
	void explore_random_child(Buffer* buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr);

	// Fonction qui renvoie le fils le plus explor�
	[[nodiscard]] Move get_most_explored_child_move(bool decide_by_eval = false);

	// Reset le noeud et ses enfants, et les supprime tous
	void reset();

	// Fonction qui renvoie les variantes d'exploration
	[[nodiscard]] string get_exploration_variants(const double alpha, const double beta, bool main = true, bool quiescence = false);

	// Fonction qui renvoie la profondeur de la variante principale
	[[nodiscard]] int get_main_depth();

	// Fonction qui renvoie le fils le plus explor�
	[[nodiscard]] Node* get_most_explored_child(bool decide_by_eval = true);

	// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
	[[nodiscard]] int get_avg_nps() const;

	// Fonction qui renvoie le nombre d'it�rations par seconde
	[[nodiscard]] int get_ips() const;

	// Quiescence search int�gr� � l'exploration
	int quiescence(Buffer* buffer, Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr, bool evaluate_threats = true, double beta_margin = 0.0);
	//void grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth);

	// Fonction qui renvoie le nombre de noeuds fils compl�tement explor�s
	[[nodiscard]] int get_fully_explored_children_count() const;

	// Fonction qui renvoie la somme des noeuds des fils
	int count_children_nodes() const;

	// Fonction qui renvoie le nombre de noeuds total
	int get_total_nodes() const;

	// Fonction qui �value la position
	void evaluate_position(Evaluator* eval, bool display = false, Network* network = nullptr, bool game_over_check = false);

	// Fonction qui renvoie un noeud fils pseudo-al�atoire (en fonction des �valuations et du nombre de noeuds)
	Move pick_random_child(const double alpha, const double beta, const double gamma);

	// Fonction qui renvoie le score d'un coup. Alpha augmente l'importance de l'�valuation, et beta augmente l'importance du winrate
	robin_map<Move, double> get_move_scores(const double alpha, const double beta, const bool consider_standpat = false, const int qdepth = -100);

	// Fonction qui renvoie la valeur du noeud
	double get_node_score(const double alpha, const double beta, const int max_eval, const double max_avg_score, const bool player, Evaluation *custom_eval = nullptr) const;

	// Fonction qui renvoie le coup avec le meilleur score
	Move get_best_score_move(const double alpha, const double beta, const bool consider_standpat = false, const int qdepth = -100);

	// Fonction qui renvoie une valeur pr�visionnelle du score du noeud, lorsqu'on ne connait pas les �valuations max (pour la quiecence)
	int get_previsonal_node_score(const double alpha, const double beta, const bool player) const;

	// Fonctions � rajouter: destruction des fils et de soi...

	// Destructeur : TODO (supprimer tous les tableaux dynamiques...)
	// Il faudra free tous les plateaux du buffer

	// Destructeur
	~Node();
};