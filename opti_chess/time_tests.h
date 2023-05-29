#pragma once
#include <time.h>
#include <iostream>
#include <string>
using namespace std;


void test_function(void (*f)(), double test_time, std::string func = __builtin_FUNCTION()); // Nom de la fonction pas vraiment le bon...