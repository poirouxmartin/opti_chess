#pragma once
#include "board.h"
#include <vector>
#include <cstddef>

// Résultat du dimensionnement des pools (nombre d'entrées)
struct PoolSizing {
	int board_length;
	int node_length;
	int tt_length;
};

// Calcule les tailles des pools depuis la RAM physique dispo au démarrage.
// ram_fraction : part de la RAM dispo = cible de RSS TOTAL du process.
// hard_cap_bytes : plafond dur du RSS total visé.
// tt_max_entries : plafond du nombre d'entrées de la table de transposition.
// rss_overhead_factor : RSS réel / taille des tableaux plats. Les tableaux
//     Board[]/Node[] ne sont pas le seul coût : chaque Node a une robin_map
//     _children, chaque Board une robin_map _positions_history, et la TT
//     elle-même grossit jusqu'à tt_max_entries — heap dynamique NON modélisé
//     par sizeof(Board)+sizeof(Node). Mesuré ≈ 2.0 (4 Go tableaux => 8 Go RSS).
//     On dimensionne donc les tableaux à budget/facteur pour que le RSS
//     TOTAL (tableaux + maps dynamiques) tienne dans le budget, sur toute
//     machine (#13 borné par construction, pas seulement les tableaux plats).
// NB: tout le calcul en unsigned long long — 4 Gio déborderait un size_t
//     32-bit (exactement le bug mémoire que ce cleanup corrige).
PoolSizing compute_pool_sizing(double ram_fraction = 0.5,
                               unsigned long long hard_cap_bytes = 4ull * 1024 * 1024 * 1024,
                               int tt_max_entries = 5000000,
                               double rss_overhead_factor = 2.0);

class BoardBuffer {
public:

	// Le buffer est-il initialisé ?
	bool _init = false;

	// Longueur du buffer
	int _length = 0;

	// Tableau de plateaux
	Board* _boards;

	// Itérateur pour rechercher moins longtemps un index de plateau libre
	int _iterator = -1;

	// Free-list : pile d'indices de plateaux libres (allocation/libération O(1))
	std::vector<int> _free_indices;

	// Vrai pendant reset()/remove() : les hooks de libération ne repoussent pas
	bool _bulk_resetting = false;

	// Le buffer est-il plein ? (O(1))
	bool is_full() const { return _free_indices.empty(); }

	// Repousse un index libéré (appelé depuis les hooks de recyclage)
	void free_index(int index) { _free_indices.push_back(index); }

	// Constructeur par défaut
	BoardBuffer();

	// Constructeur utilisant la taille (en octets) du buffer
	explicit BoardBuffer(size_t);

	// Initialize l'allocation de n plateaux
	void init(int length = 5000000, bool display = true);

	// Fonction qui donne l'index du premier plateau de libre dans le buffer
	int get_first_free_index();

	// Fonction qui désalloue toute la mémoire
	void remove();

	// Fonction qui reset le buffer
	bool reset();

	// Fonction qui renvoie le premier plateau disponible dans le buffer
	Board* get_first_free_board();

	// DEBUG *** fonction qui affiche l'état du buffer (combien de plateaux sont utilisés)
	void display_buffer_state() const;
};

// Buffer pour monte-carlo
extern BoardBuffer monte_board_buffer;