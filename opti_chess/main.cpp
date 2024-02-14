#include "main_lichess.h"
#include "main_gui.h"

bool lichess = true;

int main() {

	if (lichess)
		main_lichess();
	else
		main_ui();

}

