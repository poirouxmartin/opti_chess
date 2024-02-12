#include "main_lichess.h"
#include "main_gui.h"

bool lichess = false;

int main() {

	if (lichess)
		main_lichess();
	else
		main_ui();

}

