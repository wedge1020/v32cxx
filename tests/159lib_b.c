#include "159shared.h"
static int calls = 100;           /* private to this file, same name as in 159lib_a.c */
static int helper(int x) { calls++; return x * 2; }
static int mul(int a, int b) { return a * b + 1000; }      /* never the one in 159lib_a.c */
int next_id(void) { g_counter.n++; return g_counter.n; }
int read_total(void) { return g_total + helper(1) + calls + mul(1, 1); }
