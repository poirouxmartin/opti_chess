#pragma once
#include "board.h"
#include "gui.h"

// Test framework

// Things to test:

// Perft test, on various positions (displaying the move generation speed)
// Evaluation tests on various positions: a score based on how close the evaluation is to the real one, and on the frequency of the position *** TODO (displaying the evaluation speed)
// Problems (tests with 1 minute, 3 minutes and 10 minutes): which ones are right (problems in increasing order)
// - Tactical problems
// - Opening moves
// - Endgame problems
// - Strong strategic moves
// - Defensive moves
// Playing speed?
// Evaluation of the winning chances, and risk taking in unclear positions
// Mate search tests (displaying the search speed)
// Symmetry of the evaluation
// Missed moves
// Instinctive moves
// Playing speed
// Quiescence tests
// Evaluation speed


// TODO ***
// Write one function per test type: run_all_problems, etc...
// Write exercises by THEME, to see which theme it struggles with (themed strategic problem, evaluation of one specific parameter, etc...)
// Check on piece exchanges whether they are all evaluated correctly depending on the position

// Implementation of a score per criterion
// A global score representing the strength of the configuration (move generation, evaluation, algorithm, search parameters...)

class Tests {
public:

	// Attributes

	// Board
	//Board *_board;

	// Evaluation
	//Evaluator *_eval;

	// Algorithm

	// Search parameters

	// GUI? (to import every parameter and test straight away)
	GUI *_gui;

	// Imported tests control
	bool _imported_tests_enabled = true;
	bool _stop_imported_tests = false;

	// Enable or disable imported tests
	void set_imported_tests_enabled(bool enabled) { _imported_tests_enabled = enabled; }

	// Request stop for imported tests (can be called from UI thread)
	void stop_imported_tests() { _stop_imported_tests = true; }

	// Run imported tests from a file, returns aggregate score (0..1)
	double run_imported_tests(const string& tests_path = "Tests.txt", double time_per_puzzle = 3.0, int base_total_tests = 0, double base_total_score = 0.0);

	// Generate and run evaluation-only tests from Tests.txt and mark tested lines.
	// Returns number of tests added.
	int add_generated_evaluation_tests(const string& tests_path = "Tests.txt");

	// Constructor
	//Tests(Evaluator* eval);

	Tests(GUI *gui);


	// Functions

	// Perft test
	bool perft_test(string fen, int depth, vector<long long int> expected_nodes);

	// Returns a value between 0 and 1, 1 meaning the position is evaluated correctly
	double evaluation_test(string fen, int expected_evaluation, pair<int, int> evaluation_range, double expected_score, pair<double, double> score_range);

	// Returns a value between 0 and 1 (1 = problem solved) (should it account for the played move still being good?)
	double problem_test(string fen, robin_map<Move, double> moves, double time);

	// Updates the GUI
	void update_GUI();


	// Runs every test
	void run_all_tests();
};