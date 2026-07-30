#include "time_tests.h"


// Times the execution of a function
void test_function(void (*f)(), const double test_time, const std::string& func) {
	cout << "Testing the function '" << func << "'" << endl;

	// Start time
	const clock_t begin = clock();

	int i = 0;
	while (static_cast<double>(clock() - begin) / CLOCKS_PER_SEC < test_time) {
		f();
		i += 1;
	}

	cout << "Done in " << static_cast<double>(clock() - begin) / CLOCKS_PER_SEC / i << "s" << endl;
}