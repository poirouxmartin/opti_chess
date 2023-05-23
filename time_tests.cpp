#include "time_tests.h"


void test_function(void (*f)(), const double test_time, std::string func) {

    cout << "Testing the function '" << func << "'" << endl;

    // Temps au début
    const clock_t begin = clock();


    int i = 0;
    while (static_cast<double>(clock() - begin) / CLOCKS_PER_SEC < test_time) {
        f();
        i += 1;
    }

    cout << "Done in " << static_cast<double>(clock() - begin) / CLOCKS_PER_SEC / i << "s"  << endl;
}


