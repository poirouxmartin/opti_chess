#include "main_lichess.h"
#include "main_gui.h"
#include "main_headless.h"
#include <string>

bool lichess = false;

int main(int argc, char** argv) {

	// #11 attempt-8 — banc headless de validation DAG (design §5). N'initialise
	// jamais la fenêtre raylib ; sortie != 0 si une assertion échoue.
	if (argc > 1 && std::string(argv[1]) == "--dag-test")
		return main_headless();

	if (lichess)
		main_lichess();
	else
		main_ui();

	return 0;
}
