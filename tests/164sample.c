/* tests/164sample.c -- the small C leftovers: compound literals at file
 * scope, GNU `[lo ... hi] = v` designators, an unnamed function-typed
 * parameter, `noreturn` / `_Noreturn`. Self-checking. */
#include <stdnoreturn.h>

struct P { int x, y; };
struct P *pp = &(struct P){ 3, 4 };
int *ip = (int[]){ 5, 6, 7 };
int table[8] = { [0 ... 3] = 7, [4 ... 6] = 9, [7] = 1 };

int test_errors = -1;

noreturn void stop (int code) { for (;;) ; }
_Noreturn void stop2 (int code);
void stop3 (int code) __attribute__((noreturn));

static int twice (int v) { return v * 2; }
static int run (int f (int), int v);
static int run (int f (int), int v) { return f (v); }
static int run2 (int (int), int v);
static int run2 (int g (int), int v) { return g (v); }

int pick (int x)
{
    if (x) return 1;
    stop (1);
}

void main (void)
{
    int e = 0;
    int local[4] = { [1 ... 2] = 5 };

    if (pp->x != 3 || pp->y != 4) e |= 1;
    if (ip[0] != 5 || ip[2] != 7) e |= 2;
    if (table[0] != 7 || table[3] != 7 || table[4] != 9 || table[6] != 9 || table[7] != 1) e |= 4;
    if (local[0] != 0 || local[1] != 5 || local[2] != 5 || local[3] != 0) e |= 8;
    if (run (twice, 4) != 8) e |= 16;
    if (run2 (twice, 5) != 10) e |= 32;
    if (pick (1) != 1) e |= 64;
    test_errors = e;
}
