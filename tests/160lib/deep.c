#include "shared.h"
static int helper(int x) { return x * 100; }     /* the same static name as add.c's */
int deep(int a) { return helper(a) + 1; }
