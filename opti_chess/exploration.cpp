#include "exploration.h"
#include "useful_functions.h"
#include "zobrist.h"

// Constructeur avec un plateau, un indice et un coup
Node::Node(Board *board) {
	_board = board;
}

// Fonction qui ajoute un fils
void Node::add_child(Node* child, Move move) {
	// FIXME: vérifier si le coup n'est pas déjà dans les enfants?
	if (_children.contains(move)) {
		cout << "move already in children" << endl;
		return;
	}

	if (child == nullptr) {
		cout << "WHY DO WE ADD A NULL CHILD??" << endl;
		return;
	}

	if (move.is_null_move()) {
		cout << "null move added" << endl;
	}

	_children[move] = child;
	//_nodes += child->_nodes;
}

// Fonction qui renvoie le nombre de fils
[[nodiscard]] size_t Node::children_count() const {
	return _children.size();
}

// Fonction qui renvoie le premier coup qui n'a pas encore été ajouté
[[nodiscard]] Move Node::get_first_unexplored_move(bool fully_explored) {
	for (int i = 0; i < _board->_got_moves; i++) {
		Move move = _board->_moves[i];
		if (!_children.contains(move) || (fully_explored && !_children[move]->_fully_explored)) {
			return move;
		}
	}

	return Move();
}

// Initie le noeud en fonction de son plateau
void Node::init_node() {

	if (_initialized) {
		return;
	}

	_initialized = true;

	_board->is_game_over();

	// Si la partie est finie
	if (_board->_game_over_value != unterminated) {
		_fully_explored = true;
		_can_explore = false;
		_is_terminal = true;

		return;
	}

	// Génère et trie les coups
	_board->get_moves();
	_board->sort_moves();

	return;
}

// Nouveau GrogrosZero
void Node::grogros_zero(Buffer* buffer, Evaluator* eval, float beta, float k_add, int iterations, int quiescence_depth, Network* network) {
	// TODO:
	// On peut rajouter la profondeur
	// Garder le temps de calcul

	// BUG: rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1
	// quiescence là dessus, après il regarde à donf Tb1 après Dxh7... ??

	//cout << "toto" << endl;

	// Ne devrait pas arriver
	// FIXME!! ça arrive en fait...
	if (iterations <= 0) {
		cout << "iterations <= 0 in grogros_zero" << endl;
		return;
	}

	// Temps de calcul
	const clock_t begin_monte_time = clock();

	// INITIALISATION
	if (!_initialized) {
		quiescence(buffer, eval, quiescence_depth, -INT32_MAX, INT32_MAX, network);
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// Si la partie est finie, on ne fait rien
	if (_is_terminal) {
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// Vérifie que le buffer n'est pas plein (TODO) (et est bien initialisé aussi)

	// FIXME
	if (_board->_got_moves <= 0) {
		cout << "no moves in grogros_zero" << endl;
		return;
	}

	while (iterations > 0) {

		// EXPLORATION D'UN NOUVEAU COUP
		if (get_fully_explored_children_count() < _board->_got_moves) {
			explore_new_move(buffer, eval, quiescence_depth, network);
		}

		// EXPLORATION D'UN COUP DÉJÀ EXPLORÉ
		else {
			explore_random_child(buffer, eval, beta, k_add, quiescence_depth, network);
		}

		iterations--;
	}
	
	if (_nodes <= 0) {
		cout << "negative nodes in grogros zero???" << endl;
	}

	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	return;
}

// Fonction qui explore un nouveau coup
void Node::explore_new_move(Buffer* buffer, Evaluator* eval, int quiescence_depth, Network* network) {

	// On prend le premier coup non exploré
	const Move move = get_first_unexplored_move(true);

	// Noeud fils
	Node *child = nullptr;

	// Si on a déjà exploré ce coup, mais pas complètement
	bool already_explored = _children.contains(move);

	if (already_explored) {
		child = _children[move];

		if (_nodes <= child->_nodes) {
			cout << "child nodes >= nodes???" << endl;
		}

		_nodes -= child->_nodes; // On enlève le nombre de noeuds de ce fils

		//cout << "move already explored, but considered as a new move?" << endl;
	}
	else {
		// FIXME *** fonction pour ça !!

		// Prend une place dans le buffer
		const int buffer_index = buffer->get_first_free_index();

		// FIXME: faut peut-être check ça autre part...
		if (buffer_index == -1) {
			cout << "Buffer is full" << endl;
			return;
		}

		Board* new_board = &buffer->_heap_boards[buffer_index];
		new_board->copy_data(*_board, false, true);
		new_board->_is_active = true;
		new_board->make_move(move, false, false, true);

		// Cela transpose t-il dans une autre branche?
		new_board->get_zobrist_key();

		// TEST: 8/2k5/3p4/p2P1p2/P2P1P2/8/3K4/8 w - - 10 6
		// Quand on update un des noeuds, il faudra potentiellement backpropagate dans toutes les branches parentes...
		// Gros problème de noeuds négatifs

		bool transpositions = false;


		// Si la position est déjà dans la table de transposition
		if (transposition_table.contains(new_board->_zobrist_key) && transpositions) {
			cout << "transposition: "<< new_board->to_fen() << endl;

			child = transposition_table._hash_table[new_board->_zobrist_key]._node;
			
			cout << "transposition nodes: " << child->_nodes << endl;
			cout << "parent nodes: " << _nodes << endl;

			//_nodes += child->_nodes;

			//cout << "new transposition nodes: " << _nodes << endl;
		}

		// Sinon, on crée un nouveau noeud
		else {
			// Création du noeud fils
			child = new Node(new_board);

			// Ajoute la position dans la table
			
			if (transpositions) {
				transposition_table._hash_table[new_board->_zobrist_key] = ZobristEntry(child);
			}

			//cout << "normal" << endl;
		}
	}

	// Evalue le plateau, en regardant si la partie est finie
	//child->_board->evaluate(eval, false, nullptr, true);
	//child->_deep_evaluation._value = child->_board->quiescence(eval, -INT32_MAX, INT32_MAX, 4, true) * child->_board->get_color();
	//child->_nodes = 1;

	if (child == nullptr) {
		cout << "null child and shouldn't be (explore_new_move)" << endl;
	}

	if (!child->_fully_explored) {

		//r4rk1/ppp2ppp/2nq1b2/2np4/2P5/2NBQ2P/PP1B1PP1/R3R1K1 b - - 2 15 : ici faut jouer d4!!!

		// Stand pat: quiescence du point de vue de l'adversaire (EXPERIMENTAL)
		bool experiment = false;

		int stand_pat_eval = 0;
		
		if (experiment) {
			Board b(*child->_board);
			b._player = !b._player;
			Node* stand_pat = new Node(&b);
			stand_pat_eval = stand_pat->quiescence(buffer, eval, quiescence_depth, -INT32_MAX, INT32_MAX, network) * stand_pat->_board->get_color();

			// FIXME: prendre la meilleure eval pour standpat? (si la statique est meilleure?)
			child->_deep_evaluation._value = stand_pat_eval;
			child->_static_evaluation._value = stand_pat_eval;
			child->_deep_evaluation._value = true;
			child->_static_evaluation._value = true;
		}


		child->quiescence(buffer, eval, quiescence_depth, -INT32_MAX, INT32_MAX, network, experiment, stand_pat_eval);
		//child->_board->evaluate(eval, false, nullptr, true);
		//child->_nodes = 1;

		child->_fully_explored = true;
	}

	// rnb1kbnr/ppp1pppp/2q5/1B6/8/2N5/PPPP1PPP/R1BQK1NR b KQkq - 3 4
	// rnb1kbnr/ppp1pppp/2q5/8/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4 : ici Fb5 -> +114 au lieu de +895

	// Tous les coups ont-ils déjà été explorés?
	// FIXME: faut-il seulement évaluer si tous les coups ont été entièrement explorés?
	//bool all_moves_explored = get_fully_explored_children_count() == _board->_got_moves;
	bool all_moves_explored = children_count() + !already_explored == _board->_got_moves;

	// Met à jour l'évaluation du plateau
	if (!all_moves_explored) {
		if (_board->_player) {
			if (child->_deep_evaluation._value > _deep_evaluation._value) {
				_deep_evaluation = child->_deep_evaluation;
			}
		}
		else {
			if (child->_deep_evaluation._value < _deep_evaluation._value) {
				_deep_evaluation = child->_deep_evaluation;
			}
		}
	}

	// Tous les coups ont été explorés, donc on met à jour l'évaluation du plateau avec le meilleur coup
	else {
		Evaluation best_eval = Evaluation();

		for (auto const& [move_2, child_2] : _children) {
			if (_board->_player ? child_2->_deep_evaluation > best_eval : child_2->_deep_evaluation < best_eval) {
				best_eval = child_2->_deep_evaluation;
			}
		}

		_deep_evaluation = best_eval;
	}

	// FIXME optimiser ces cas pour n'en calculer qu'un

	// Si tous les coups ont été explorés, on met à jour l'évaluation du plateau avec le meilleur coup
	//if (children_count() == _board->_got_moves) {
	//	int color = _board->get_color();
	//	int best_eval = -INT_MAX;

	//	for (auto& [move, child] : _children) {
	//		if (child->_deep_evaluation._value * color > best_eval) {
	//			best_eval = child->_deep_evaluation._value * color;
	//		}
	//	}

	//	_deep_evaluation._value = best_eval * color;
	//}
	

	// FIXME: si on a regardé tous les fils, et qu'aucun des coups n'améliore l'évaluation, on fait quoi?
	// - Option 1: on garde l'évaluation sans aucun coup
	// - Option 2: on garde l'évaluation du meilleur coup
	// Est-ce vraiment grave? peut-être pas car si on continue une profondeur plus loin, explore_random_child() va prendre le meilleur coup

	// Augmente le nombre de noeuds
	//_nodes++;
	//cout << "main nodes:" << _nodes << ", child nodes:" << child->_nodes << endl;
	_nodes += child->_nodes;

	if (_nodes <= 0) {
		cout << "negative nodes in explore_new_move???" << endl;
	}

	if (child->_nodes <= 0) {
		cout << "negative nodes in explore_new_move child???" << endl;
	}

	//cout << "total: " << _nodes << endl;
	child->_iterations = 1;
	child->_chosen_iterations = 1;
	_iterations++;

	// Ajoute le fils
	if (!already_explored) {
		//cout << "new child" << endl;
		add_child(child, move);
	}
	else {
		//cout << "ALREADY EXPLORED! should we add nodes here??" << endl;
		// FIXME: this should be reviewed
	}
}

// Fonction qui explore dans un plateau fils pseudo-aléatoire
void Node::explore_random_child(Buffer* buffer, Evaluator* eval, float beta, float k_add, int quiescence_depth, Network* network) {

	// Prend un fils aléatoire
	const Move move = pick_random_child(beta, k_add);
	Node *child = _children[move];

	if (child->_nodes >= _nodes) {
		cout << "child nodes >= nodes in random exploration???" << endl;
	}

	// Nombre de noeuds du fils
	const int initial_child_nodes = child->_nodes;

	// On explore ce fils
	child->grogros_zero(buffer, eval, beta, k_add, 1, quiescence_depth, network); // L'évaluation du fils est mise à jour ici

	// Met à jour l'évaluation du plateau
	Evaluation best_eval = Evaluation();

	for (auto const& [move_2, child_2] : _children) {
		if (_board->_player ? child_2->_deep_evaluation > best_eval : child_2->_deep_evaluation < best_eval) {
			best_eval = child_2->_deep_evaluation;
		}
	}

	_deep_evaluation = best_eval;

	// Augmente le nombre de noeuds
	_nodes += child->_nodes - initial_child_nodes;

	if (_nodes <= 0) {
		cout << "negative nodes in explore_random_child???" << endl;
	}

	if (child->_nodes <= 0) {
		cout << "negative nodes in explore_random_child child???" << endl;
	}

	// Augmente le nombre d'itérations
	_iterations++;
}

// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variantes, en fonction de la grandeur du nombre correspondant à cet index
Move Node::pick_random_child(const float beta, const float k_add) {
	// FIXME: ajouter quelque chose comme l'évaluation relative pour choisir les coups plutôt que les évaluations brutes... sinon, dans les évaluations énormes, il regarde un seul coup...

	const int color = _board->get_color();
	auto n_children = static_cast<int>(children_count());

	if (n_children <= 0) {
		cout << "no children in pick_random_child" << endl;
	}

	// *** 1. ***
	// 
	// Evaluations des enfants (en fonction de la couleur)
	double evaluations[100]{};

	// Score moyen des enfants
	float avg_scores[100]{};

	// Coups
	map<int, Move> children_moves;

	int i = 0;
	for (auto const& [move, child] : _children) {
		evaluations[i] = color * child->_deep_evaluation._value;
		avg_scores[i] = _board->_player ? child->_deep_evaluation._avg_score : 1 - child->_deep_evaluation._avg_score;
		children_moves[i] = move;
		i++;
	}

	//cout << "new: " << endl;
	//print_array(l2, n_children);

	// Plus les évaluations sont élevées, plus on veut regarder large (EXPERIMENTAL)
	
	// Facteur d'élargissement de la distribution
	//double enlargement_factor = pow(log10(1.0 + abs(max_eval / 10)), 2.0) + 1.0;
	// Re: diminuer les valeurs des checks? à voir...
	// Re: augmenter la valeur des pièces?
	//double enlargement_factor = 1.0 + abs(max_eval / 100);
	if (_nodes <= 0) {
		cout << "negative nodes???" << endl;
	}

	// Faut-il chercher plus large en finale?
	//double enlargement_factor = (double)_nodes / (double)_iterations;
	double enlargement_factor = pow((double)_nodes / (double)_iterations / (1 - _board->_adv + 0.5f), 0.1);
	//double enlargement_factor = 1;
	
	// FIXME: quelle est la meilleure manière d'élargir?
	// Réduire beta? Augmenter k_add? Les deux?
	float beta_enlarged = beta / enlargement_factor;
	float k_add_enlarged = k_add * enlargement_factor;

	//cout << "eval: " << abs(max_eval) << ", enlargement factor: " << enlargement_factor << ", beta: " << beta << ", beta_enlarged: " << beta_enlarged << ", k_add: " << k_add << ", k_add_enlarged: " << k_add_enlarged << endl;

	// Softmax sur les évaluations
	softmax(evaluations, avg_scores, n_children, beta_enlarged, k_add_enlarged);

	//print_array(l2, n_children);

	// *** 2. ***
	// Liste de pondération en fonction de l'exploration de chaque noeud (donne plus de poids aux noeuds moins explorés)
	double pond[100]{};

	i = 0;
	for (auto const& [_, child] : _children) {
		//if (_nodes < 0 || child->_nodes < 0)
		//	cout << "negative nodes!!!!!!!!!!!!!" << endl;
		//if (_nodes == 0 || child->_nodes == 0) {
		//	cout << "0 nodes???" << endl;
		//}
		if (child->_iterations == 0) {
			cout << "0 iterations???" << endl;
		}
		pond[i] = child->_chosen_iterations == 0 ? INT_MAX : static_cast<double>(_iterations) / static_cast<double>(child->_chosen_iterations);
		i++;
	}

	//print_array(pond, n_children);

	// On applique la pondération aux évaluations
	nodes_weighting(evaluations, pond, n_children);

	//print_array(l2, n_children);

	// *** 3. ***
	// Somme de toutes les valeurs
	double sum = 0.0;

	for (int i = 0; i < n_children; i++)
	{
		sum += evaluations[i];
	}

	// Choix du coup en fonction d'une valeur aléatoire
	if (sum > LLONG_MAX) {
		cout << "sum too big" << endl;
	}

	//rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2

	// Choix initial du coup
	const double rand_val = rand_double(0.0, sum);
	double cumul = 0.0;

	for (int k = 0; k < n_children; k++)
	{
		cumul += evaluations[k];

		// Coup choisi
		if (cumul >= rand_val) {
			//cout << "move index: " << i << endl;
			//cout << "move: " << _board->move_label(children_moves[i]) << endl;

			_children.at(children_moves[k])->_chosen_iterations++;
			
			// S'il n'y a plus rien à explorer sur cette branche...
			if (_children.at(children_moves[k])->_can_explore) {
				return children_moves[k];
			}
			else {
				//cout << "no more moves to explore" << endl;
				break;
			}
		}
	}


	// Refait un choix des coups en fonction des branches explorables

	double new_sum = 0.0;

	for (int i = 0; i < n_children; i++)
	{
		if (_children.at(children_moves[i])->_can_explore) {
			new_sum += evaluations[i];
		}
	}

	if (new_sum == 0) {
		cout << "no more moves to explore" << endl;
		_can_explore = false;
	}

	const double new_rand_val = rand_double(0.0, new_sum);
	if (new_rand_val > new_sum) {
		cout << "???" << endl;
	}

	double new_cumul = 0.0;

	for (int k = 0; k < n_children; k++)
	{
		if (_children.at(children_moves[k])->_can_explore) {
			new_cumul += evaluations[k];

			// Nouveau coup choisi
			if (new_cumul >= new_rand_val) {
				return children_moves[k];
			}
		}
	}

	if (_can_explore) {
		cout << "first move chosen by default?" << endl;
		print_array(evaluations, n_children);
		cout << "sum: " << sum << ", rand: " << rand_val << endl;
	}


	return children_moves[0];
}

// Fait un softmax sur les evaluations
void Node::softmax(double* evaluations, float* avg_scores, int size, const float beta, const float k_add) const {

	// Constante multiplicative pour l'exponentielle
	constexpr double r = 100000;

	// Evaluation maximale
	const double best_eval = *max_element(evaluations, evaluations + size);

	// Score moyen maximal
	const float best_avg_score = *max_element(avg_scores, avg_scores + size);


	// Somme des exponentielles
	double eval_sum = 0.0;

	for (int i = 0; i < size; i++)
		eval_sum += exp(beta * (evaluations[i] - best_eval));

	// Constante de calcul
	const double constant = beta * best_eval + log(eval_sum);


	// Pour chaque évaluation
	for (int i = 0; i < size; i++)
	{
		const double eval = evaluations[i];
		const double avg_score = avg_scores[i];
		const double adding = (avg_score == 0.0f) ? 0.0f : avg_score / best_avg_score;

		// TODO *** formule à revoir
		evaluations[i] = r * exp(beta * eval - constant) + k_add * adding;
	}
}

// Fonction qui renvoie le fils le plus exploré
[[nodiscard]] Move Node::get_most_explored_child_move(bool decide_by_eval) {
	int max = 0;

	// Tri simple, on ne départage pas les égalités
	if (!decide_by_eval) {

		auto best_move = Move();

		for (auto const& [move, child] : _children) {
			if (child->_chosen_iterations > max) {
				max = child->_chosen_iterations;
				best_move = move;
			}
		}

		return best_move;
	}

	// Avec un départage par égalité
	else {
		vector<Move> max_iterations_moves;
		int color = _board->get_color();

		for (auto const& [move, child] : _children) {
			if (child->_chosen_iterations == max) {
				max_iterations_moves.push_back(move);
			}
			else if (child->_chosen_iterations > max) {
				max = child->_chosen_iterations;
				max_iterations_moves.clear();
				max_iterations_moves.push_back(move);
			}
		}

		// En cas d'égalité, on trie par évaluation
		long long int max_eval = -LLONG_MAX;
		auto best_move = Move();

		for (auto const& move : max_iterations_moves) {
			Node const* child = _children[move];

			if (child->_deep_evaluation._value * color > max_eval) {
				max_eval = child->_deep_evaluation._value * color;
				best_move = move;
			}
		}

		return best_move;
	}
}

// Reset le noeud et ses enfants, et les supprime tous
void Node::reset() {
	//cout << "resetting node" << endl;
	_latest_first_move_explored = -1;
	_nodes = 0;
	_iterations = 0;
	_chosen_iterations = 0;
	_board->reset_board();
	_initialized = false;
	_is_terminal = false;
	_can_explore = true;
	_time_spent = 0;
	_fully_explored = false;
	//cout << "resetting children" << endl;

	for (auto const& [_, child] : _children) {
		child->reset();
	}

	//cout << "clearing children" << endl;
	_children.clear();

	//cout << "done" << endl;
}

// Fonction qui renvoie les variantes d'exploration
string Node::get_exploration_variants(bool main, bool quiescence) {

	// Si on est en fin de variante
	if (_board->_game_over_value) {
		return "";
	}

	string variants = "";

	// S'il y a des coups explorés
	if (children_count() > 0) {

		// Si on est dans le noeud principal, on affiche toutes les variantes
		if (main) {
			// TODO: améliorer ce tri...

			// Trie les enfants par nombre d'itérations par l'algo de GrogrosZero
			vector<pair<int, Move>> children_iterations;

			for (auto const& [move, child] : _children) {
				if (child == nullptr) {
					// FIXME
					cout << "null child" << endl;
					children_iterations.push_back(make_pair(0, move)); // On met un moins pour trier dans l'ordre décroissant
				}
				else {
					children_iterations.push_back(make_pair(-child->_chosen_iterations, move)); // On met un moins pour trier dans l'ordre décroissant
				}
			}

			std::ranges::sort(children_iterations.begin(), children_iterations.end());

			// En cas d'égalité, on trie par évaluation
			vector<Move> children_moves;

			vector<pair<int, Move>> children_evaluations;

			int color = _board->get_color();

			children_evaluations.push_back(make_pair(-_children[children_iterations[0].second]->_deep_evaluation._value * color, children_iterations[0].second));

			for (int i = 1; i < children_count() + 1; i++) {

				// Fin des égalités, on trie par évaluation
				if (i == children_count() || children_iterations[i].first != children_iterations[i - 1].first) {
					std::ranges::sort(children_evaluations.begin(), children_evaluations.end());
					for (auto const& [eval, move] : children_evaluations) {
						children_moves.push_back(move);
					}

					children_evaluations.clear();

					// Enfant à trier
					if (i < children_count()) {
						children_evaluations.push_back(make_pair(-_children[children_iterations[i].second]->_deep_evaluation._value * color, children_iterations[i].second));
					}
				}
				else {
					children_evaluations.push_back(make_pair(-_children[children_iterations[i].second]->_deep_evaluation._value * color, children_iterations[i].second));
				}
			}

			for (int i = 0; i < children_moves.size(); i++) {
				const Move move = children_moves[i];
				Node* child = _children[move];

				const int child_iterations = child->_iterations;
				const int child_chosen_iterations = child->_chosen_iterations;
				const bool new_quiescence = !quiescence && child_iterations == 0;

				variants += "eval: " + _board->evaluation_to_string(child->_deep_evaluation._value) + " | ";
				variants += (new_quiescence ? "(" : "") + to_string(_board->_moves_count) + (_board->_player ? ". " : "... ") + _board->move_label(move, true) + " " + child->get_exploration_variants(false, new_quiescence || quiescence) + (new_quiescence ? ")" : "")+ "\n";

				const int nodes = _nodes;
				const int child_nodes = child->_nodes;
				const int nodes_ratio = _nodes == 0 ? 0 : child_nodes * 100 / nodes;
				if (_nodes == 0) {
					cout << "nodes == 0?? le bug est peut-être ici..." << endl;
				}

				const int iterations = _iterations;
				const int iterations_ratio = _iterations == 0 ? 0 : child_iterations * 100 / iterations;

				const int chosen_iterations_ratio = iterations == 0 ? 0 : child_chosen_iterations * 100 / iterations;

				variants += "C: " + int_to_round_string(child_chosen_iterations) + " (" + int_to_round_string(chosen_iterations_ratio) + "%) | I: " + int_to_round_string(child_iterations) + " (" + int_to_round_string(iterations_ratio) + "%) | N: " + int_to_round_string(child_nodes) + " (" + int_to_round_string(nodes_ratio) + "%) | D: " + int_to_round_string(child->get_main_depth() + 1) + " | T: " + clock_to_string(child->_time_spent, true) + "\n\n";
			}
		}

		// Sinon, on affiche seulement le coup le plus exploré
		else {
			// Affiche seulement le premier coup (le plus exploré, et en cas d'égalité, celui avec la meilleure évaluation)
			const Move best_move = get_most_explored_child_move();

			const bool new_quiescence = !quiescence && _children[best_move]->_iterations == 0;

			variants += (new_quiescence ? "(" + _board->evaluation_to_string(_deep_evaluation._value) + ") (" : "") + (_board->_player ? to_string(_board->_moves_count) + ". " : "") + _board->move_label(best_move, true) + " " + _children[best_move]->get_exploration_variants(false, new_quiescence || quiescence) + (new_quiescence ? ")" : "");
		}
		
	}

	// S'il n'y a pas de coups explorés
	else {

		// On affiche l'évaluation du plateau en fin de variante
		variants = "(" + _board->evaluation_to_string(_deep_evaluation._value) + ")";
	}
	
	return variants;
}

// Fonction qui renvoie la profondeur de la variante principale
[[nodiscard]] int Node::get_main_depth() {
	if (children_count() > 0) {
		Move main_move = get_most_explored_child_move();

		if (main_move.is_null_move()) {
			cout << "most_explored move is null???" << endl;
			return 0;
		}

		Node* main_child = _children[main_move];

		return main_child->get_main_depth() + 1;
	}

	return 0;
}

// Destructeur
Node::~Node() {
	//cout << "destructor not implemented" << endl;
	//for (int i = 0; i < children_count(); i++) {
	//	delete _children[i];
	//}
}

// Fonction qui renvoie le meilleur coup
[[nodiscard]] Move Node::get_best_move() {
	return get_most_explored_child_move();
}

// Fonction qui renvoie le fils le plus exploré
[[nodiscard]] Node* Node::get_most_explored_child(bool decide_by_eval) {
	return _children[get_most_explored_child_move(decide_by_eval)];
}

// Fonction qui renvoie la vitesse de calcul moyenne en noeuds par seconde
[[nodiscard]] int Node::get_avg_nps() const {
	return _time_spent == 0 ? 0 : ((double)_nodes / (double)_time_spent * CLOCKS_PER_SEC);
}

// Fonction qui renvoie le nombre d'itérations par seconde
[[nodiscard]] int Node::get_ips() const {
	return _time_spent == 0 ? 0 : ((double)_iterations / (double)_time_spent * CLOCKS_PER_SEC);
}

// Quiescence search intégré à l'exploration
int Node::quiescence(Buffer* buffer, Evaluator* eval, int depth, int alpha, int beta, Network* network, bool custom_stand_pat, int stand_pat_value) {
	// TODO: comment gérer la profondeur? faire en fonction de l'importance de la branche?
	// mettre aucune profondeur limite?
	// pourquoi en endgame ça va si loin? il fait full échecs...

	// YA DES BUGS DE PRUNING...
	//r1bqr2k/1pp2p1B/p3p2Q/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 3 26
	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : #2......
	//r1bqr1k1/1pp2p1Q/p3p1B1/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 b - - 6 27 ... ? il regarde pas plus loin?? il affiche pas #1 comme éval...

	// r4rk1/ppp2ppp/2nq1b2/2n5/2Pp4/2NBQ2P/PP1B1PP1/R3R1K1 w - - 0 16 : ??????????????

	//cout << "depth: " << depth << endl;
	//rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3

	// On a au moins évalué le plateau du noeud
	if (_nodes > 0) {
		//cout << "quiescence nodes from already explored:" << _nodes << endl;
	}
	else {
		_nodes = 1;
	}

	// Temps de calcul
	const clock_t begin_monte_time = clock();

	//cout << _new_node << ", " << _board->_evaluated << ", " << (int)_board->_got_moves << ", " << (int)_board->_sorted_moves << ", " << _board->_game_over_checked << endl;

	// Initialisation du noeud
	init_node();

	// Couleur du joueur
	int color = _board->get_color();

	// Evalue la position
	if (!_static_evaluation._evaluated) {
		evaluate_position(eval, false, network, true);
	}
	else {
		_deep_evaluation = _static_evaluation;
	}

	// Si la partie est finie
	if (_is_terminal) {
		_nodes = 1;
		_iterations = 1;
		_time_spent += clock() - begin_monte_time;

		return _deep_evaluation._value * color;
	}

	// Stand pat
	int stand_pat = (custom_stand_pat ? stand_pat_value : _deep_evaluation._value) * color;

	// Si on est en échec (pour ne pas terminer les variantes sur un échec)
	bool in_check = _board->in_check();

	//r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 : le check extension fonctionne pas?

	// Profondeur nulle, on renvoie le standpat
	if (depth <= 0 && !in_check) {
		_time_spent += clock() - begin_monte_time;
		//cout << "stand pat" << endl;
		return stand_pat;
	}


	// Beta cut-off 
	// FIXME: ça casse un peu tout
	bool test_full_checks = false;

	if (stand_pat >= beta && (!in_check || !test_full_checks)) {
		_time_spent += clock() - begin_monte_time;
		//cout << "beta cut-off1: " << stand_pat << " >= " << beta << endl;
		return beta;
	}
	else {
		//cout << "no beta cut-off1: " << stand_pat << " < " << beta << endl;
	}

	// Mise à jour de alpha si l'éval statique est plus grande
	if (stand_pat > alpha && !in_check) {
	//if (stand_pat > alpha) {
		alpha = stand_pat;
	}


	// Regarde toutes les captures
	// FIXME: faudra regarder que les coups non explorés? (ou gérer avec la profondeur...)
	// TODO: get_next_capture()?
	for (int i = 0; i < _board->_got_moves; i++) {

		// Coup
		const Move move = _board->_moves[i];

		// Ce coup a-t-il déjà été exploré?
		//int child_index = get_child_index(move);
		//bool already_explored = child_index != -1;

		bool already_explored = _children.contains(move);

		//if (already_explored) {
		//	cout << "move already explored" << endl;
		//	// FIXME: IL FAUT GERER CE CAS-LA!!!
		//	continue;
		//}

		// *** FAUT-IL EXPLORER CE COUP? ***
		bool should_explore = false;

		// Si on est en échec, on explore tous les coups
		if (in_check) {
			should_explore = true;
		}
		else {
			// Si c'est une capture
			if (_board->_array[move.i2][move.j2] != none) {
				should_explore = true;
			}
			// Si c'est un roque (EXPÉRIMENTAL)
			//else if ((_board->_array[move.i1][move.j1] == 6 && abs(move.j1 - move.j2) == 2) || (_board->_array[move.i1][move.j1] == 12 && abs(move.j1 - move.j2) == 2)) {
			//	should_explore = true;
			//}
			else {
				// Si c'est une promotion
				if ((_board->_array[move.i1][move.j1] == w_pawn && move.i2 == 7) || (_board->_array[move.i1][move.j1] == b_pawn && move.i2 == 0)) {
					should_explore = true;
				}
				// Si le coup met en échec
				else {
					Board b(*_board);
					b.make_move(move);
					should_explore = b.in_check();
				}
			}
		}

		/*if (should_explore)
			cout << "depth: " << depth << " | move: " << _board->move_label(move) << " | check_extension: " << check_extension << endl;*/

		// *** EXPLORATION ***
		// Si c'est une capture/échec/promotion, on explore
		if (should_explore) {

			Node *child = nullptr;

			// Si on a déjà exploré ce coup, mais pas complètement
			if (already_explored) {
				//cout << "move already explored" << endl;

				child = _children[move];
			}

			else {
				// Prend une place dans le buffer
				const int buffer_index = buffer->get_first_free_index();

				// FIXME: faut peut-être check ça autre part...
				if (buffer_index == -1) {
					cout << "Buffer is full" << endl;
					_time_spent += clock() - begin_monte_time;

					return alpha; // faut renvoyer quoi?
				}

				Board* new_board = &buffer->_heap_boards[buffer_index];
				new_board->copy_data(*_board, false, true);
				new_board->_is_active = true;
				new_board->make_move(move, false, false, true);

				// Création du noeud fils
				child = new Node(new_board);
				//cout << get_child_index(move) << endl;
				add_child(child, move);
			}

			

			//cout << "beta: " << beta << " | alpha: " << alpha << endl;

			// Appel récursif sur le fils
			int score = - child->quiescence(buffer, eval, depth - 1, -beta, -alpha, network);
			//child->grogros_quiescence(buffer, eval, depth - 1);
			_nodes += child->_nodes;

			// Mise à jour de l'évaluation du plateau
			// FIXME: ici, s'il y'a un seul coup, il faut mettre à jour l'évaluation du plateau même si l'évaluation fils est moins bonne
			// Ou alors: si tous les coups ont été explorés, on met à jour l'évaluation du plateau avec le meilleur coup
			//bool all_moves_explored = get_fully_explored_children_count() == _board->_got_moves;
			bool all_moves_explored = children_count() == _board->_got_moves;

			if (all_moves_explored) { // FIXMEEEEEEEEE
				Evaluation best_eval = Evaluation();

				for (auto const& [move_2, child_2] : _children) {
					if (_board->_player ? child_2->_deep_evaluation > best_eval : child_2->_deep_evaluation < best_eval) {
						best_eval = child_2->_deep_evaluation;
					}
				}

				_deep_evaluation = best_eval;
			}
			else {
				if (_board->_player ? child->_deep_evaluation._value > _deep_evaluation._value : child->_deep_evaluation._value < _deep_evaluation._value) {
					_deep_evaluation = child->_deep_evaluation;
				}
			}

			// Beta cut-off
			if (score >= beta) {
				_time_spent += clock() - begin_monte_time;
				//cout << "move: " << _board->move_label(move) << " | beta cut-off2: " << score << " >= " << beta << endl;
				return beta;
			}

			// Mise à jour de alpha si l'éval statique est plus grande
			if (score > alpha) {
				alpha = score;
			}

			
		}
	}

	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	//cout << "alpha: " << alpha << endl;
	//_deep_evaluation._value = alpha * color;

	return alpha;
}

// Fonction qui renvoie le nombre de noeuds fils complètement explorés
[[nodiscard]] int Node::get_fully_explored_children_count() const {
	int count = 0;

	for (auto const& [_, child] : _children) {
		if (child->_fully_explored) {
			count++;
		}
	}

	return count;
}

// Fonction qui renvoie la somme des noeuds des fils
int Node::count_children_nodes() const {
	int sum = 0;

	for (auto& [move, child] : _children) {
		sum += child->get_total_nodes();
	}

	cout << "SUM: " << sum << endl;

	return sum;
}

// Fonction qui renvoie le nombre de noeuds total
// TODO: à utiliser seulement pour savoir si le buffer est plein
int Node::get_total_nodes() const {
	return count_children_nodes() + 1;
}

// Fonction qui évalue la position
void Node::evaluate_position(Evaluator* eval, bool display, Network * network, bool game_over_check) {
	_board->evaluate(eval, display, network, game_over_check);
	_deep_evaluation._value = _board->_evaluation;
	_deep_evaluation._wdl = _board->_wdl;
	_deep_evaluation._avg_score = _board->get_average_score();
	_deep_evaluation._evaluated = true;

	_static_evaluation = _deep_evaluation;
}