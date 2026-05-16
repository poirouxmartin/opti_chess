#pragma once

#include "board.h"
#include "buffer.h"
#include <robin_map.h>
#include <robin_hash.h>
#include <vector>

using namespace tsl;
using PositionHistory = RepetitionHistory;

class Node;

struct ChildLink {
	Node* _node = nullptr;
	int _chosen_iterations = 0;
	int _propagated_nodes = 0;
};

// TODO:
// Au lieu d'avoir un plateau, stocker seulement l'indice du plateau dans le buffer?

// Noeud de l'arbre d'exploration.
//
// Important pour les repetitions et les futures transpositions:
// - le noeud stocke l'etat "positionnel" de l'exploration (evaluations, enfants, compteurs),
// - l'historique des positions repetees reste volontairement hors du noeud et est passe
//   par la pile d'appels, car il depend du chemin courant et non de la position seule.
class Node {
public:

	// Variables

	// Plateau : FIXME -> indice du plateau dans le buffer?
	Board* _board = nullptr;

	// Coup jou� pour arriver � ce plateau (FIXME: est-ce d�j� stock� dans le plateau?)
	//Move _move;

	// Fils avec leur coup associe.
	// Les stats de selection et les compteurs de noeuds propages vivent sur l'arete pour
	// rester corrects quand plusieurs parents partagent un meme noeud.
	robin_map<Move, ChildLink> _children;

	// Fils
	//vector<Node*> _children;

	// Pour acc�l�rer la recherche du premier coup non explor�
	int _latest_first_move_explored = -1;

	// Nombre de noeuds dans le sous-arbre courant.
	// Ce compteur est tree-local : il n'est correct que tant qu'un noeud n'est pas partage
	// entre plusieurs parents.
	int _nodes = 0;
	//int _nodes = count_children_nodes() + 1;

	// Nombre d'explorations par l'algorithme de GrogrosZero
	int _iterations = 0;

	// Nombre de parents qui referencent ce noeud.
	int _parent_count = 0;

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

	// Pour savoir si il est actif dans le buffer
	bool _is_active = false;

	// Index dans monte_node_buffer (-1 = objet hors buffer : ne pas recycler)
	int _buffer_index = -1;

	// A rajouter : �valuation?, nombre de noeuds?...

	// Constructeurs

	// Constructeur
	Node();

	// Constructeur avec un plateau
	Node(Board* board);


	// Fonctions

	// Fonction qui ajoute un fils
	void add_child(Node* child, Move move);

	// Fonction qui renvoie le nombre de fils
	size_t children_count() const;

	// Fonction qui renvoie l'indice du fils associ� au coup s'il existe, -1 sinon (dans le vecteur _children)
	//int get_child_index(Move move) const;

	// Fonction qui renvoie l'indice du premier coup qui n'a pas encore �t� ajout�, -1 sinon
	//int get_first_unexplored_move_index(bool fully_explored = false);

	// Fonction qui renvoie le premier coup qui n'a pas encore �t� ajout�
	Move get_first_unexplored_move(bool fully_explored = false);

	// Initie le noeud en fonction de son plateau
	void init_node();

	// Nouveau GrogrosZero
	void grogros_zero(BoardBuffer* board_buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int nodes, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr);

	// Fonction qui explore un nouveau coup
	void explore_new_move(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr);

	// Fonction qui explore dans un plateau fils pseudo-al�atoire
	void explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr);

	// Fonction qui renvoie le fils le plus explor�
	Move get_most_explored_child_move();

	// Reset le noeud et ses enfants, et les supprime tous
	void reset(bool recursive = true);

	// Fonction qui renvoie les variantes d'exploration
	string get_exploration_variants(const double alpha, const double beta, bool main = true, bool quiescence = false, int max_depth = 500);

	// Fonction qui renvoie la profondeur de la variante principale
	int get_main_depth(const double alpha, const double beta, int max_depth = 500);

	// Fonction qui renvoie le fils le plus explor�
	Node* get_most_explored_child();

	// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
	int get_avg_nps() const;

	// Fonction qui renvoie le nombre d'it�rations par seconde
	int get_ips() const;

	// Quiescence search int�gr� � l'exploration
	int quiescence(BoardBuffer* board_buffer, Evaluator* evaluator, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr, bool evaluate_threats = true, int beta_margin = 0, const PositionHistory *path_history = nullptr);
	//void grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth);

	// Fonction qui renvoie le nombre de noeuds fils compl�tement explor�s
	int get_fully_explored_children_count() const;

	// Fonction qui renvoie la somme des noeuds des fils
	int count_children_nodes() const;

	// Fonction qui renvoie le nombre de noeuds total
	int get_total_nodes() const;

	// Fonction qui �value la position
	void evaluate_position(Evaluator* evaluator, bool display = false, Network* network = nullptr, bool game_over_check = true, bool static_only = false);

	// Fonction qui renvoie un noeud fils pseudo-al�atoire (en fonction des �valuations et du nombre de noeuds)
	Move pick_random_child(const double alpha, const double beta, const double gamma);

	// Fonction qui renvoie le score d'un coup. Alpha augmente l'importance de l'�valuation, et beta augmente l'importance du winrate
	robin_map<Move, double> get_move_scores(const double alpha, const double beta, const bool consider_standpat = false, const int qdepth = -100) const;

	// Fonction qui renvoie la valeur du noeud
	double get_node_score(const double alpha, const double beta, const int max_eval, const double max_avg_score, const bool player, Evaluation *custom_eval = nullptr) const;

	// Fonction qui renvoie le coup avec le meilleur score
	Move get_best_score_move(const double alpha, const double beta, const bool consider_standpat = false, const int qdepth = -100);

	// Fonction qui renvoie une valeur pr�visionnelle du score du noeud, lorsqu'on ne connait pas les �valuations max (pour la quiecence)
	int get_previsonal_node_score(const double alpha, const double beta, const bool player) const;

	// Fonction qui �value la menace en utilisant une quiesence sur le tour de l'adversaire
	int evaluate_quiescence_threat(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr) const;

	// Quiescence minimale (sans stockage des noeuds)
	int minimal_quiescence(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr);

	// Fonctions � rajouter: destruction des fils et de soi...

	// Destructeur : TODO (supprimer tous les tableaux dynamiques...)
	// Il faudra free tous les plateaux du buffer

	// Destructeur
	~Node();
};


class NodeBuffer {
public:

	// Le buffer est-il initialis� ?
	bool _init = false;

	// Longueur du buffer
	int _length = 0;

	// Tableau de plateaux
	Node* _nodes;

	// It�rateur pour rechercher moins longtemps un index de plateau libre
	int _iterator = -1;

	// Free-list : pile d'indices de noeuds libres (allocation/libération O(1))
	std::vector<int> _free_indices;

	// Vrai pendant reset()/remove() : les hooks de libération ne repoussent pas
	bool _bulk_resetting = false;

	// Le buffer est-il plein ? (O(1))
	bool is_full() const { return _free_indices.empty(); }

	// Repousse un index libéré (appelé depuis les hooks de recyclage)
	void free_index(int index) { _free_indices.push_back(index); }

	// Constructeur par d�faut
	NodeBuffer();

	// Constructeur utilisant la taille (en octets) du buffer
	explicit NodeBuffer(size_t);

	// Initialize l'allocation de n plateaux
	void init(int length = 10000000, bool display = true);

	// Fonction qui donne l'index du premier plateau de libre dans le buffer
	int get_first_free_index();

	// Fonction qui d�salloue toute la m�moire
	void remove();

	// Fonction qui reset le buffer
	bool reset();

	// Fonction qui renvoie le premier noeud disponible dans le buffer
	Node* get_first_free_node();

	// DEBUG *** fonction qui affiche l'�tat du buffer (combien de noeuds sont utilis�s)
	void display_buffer_state() const;
};

extern NodeBuffer monte_node_buffer;
