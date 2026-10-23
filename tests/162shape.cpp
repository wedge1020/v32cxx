#include "162shape.hpp"
static int calls = 100;
int g_total = 0;
Counter::Counter(int start) : value(start) {}
int Counter::next() { calls++; g_total += calls; return value++; }
int twice(int v) { return v * 2; }
