#include "159shared.h"
int g_total = 5;
struct Counter g_counter;
int table[4] = { 10, 20, 30, 40 };
static int calls;                 /* private to this file */
static int helper(int x) { calls++; return x + 1; }
int add(int a, int b) { return helper(a) + b + twice(0); }
int a_calls(void) { return calls; }
static int mul(int a, int b) { return a * b; }
int (*ops[2])(int, int) = { add, mul };
