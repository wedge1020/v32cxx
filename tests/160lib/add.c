#include "shared.h"
int lib_counter = 10;
static int helper(int x) { return x + 1; }
int add(int a, int b) { return helper(a) + b; }
