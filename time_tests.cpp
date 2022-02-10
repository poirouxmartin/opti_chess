#include "time_tests.h"



void test_function(void (*f)(), double test_time) {

    cout << "Testing the function '" << "__func__" << "'" << endl;

    // Temps au début
    clock_t begin = clock();


    int i = 0;
    while ((double)(clock() - begin) / CLOCKS_PER_SEC < test_time) {
        f();
        i += 1;
    }

    cout << "Done in " << (double)(clock() - begin) / CLOCKS_PER_SEC / i << "s"  << endl;
}


