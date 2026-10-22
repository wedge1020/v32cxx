/* tests/159sample.c -- linking several files into one program (gap G6):
 * v32c 159sample.c 159lib_a.c 159lib_b.c.
 * Prototypes and `extern` declarations bind to the definition in another
 * file; the three `static` names that every file defines (calls, helper,
 * mul) stay private to their own file. Self-checking: test_errors must
 * end at 0. */
#include "159shared.h"
int test_errors = -1;

static int calls = 1000;          /* a third `calls` */

void main (void)
{
    int e = 0;
    int i;

    if (add (1, 2) != 4) e |= 1;                  /* helper(1)=2, +2 */
    if (a_calls () != 1) e |= 2;                  /* 159lib_a.c's calls */
    if (next_id () != 1 || next_id () != 2) e |= 4;
    g_total = 7;
    /* b: 7 + helper(1)=2 + calls 101 + mul(1,1)=1001 */
    if (read_total () != 7 + 2 + 101 + 1001) e |= 8;
    for (i = 0; i < 4; i++) if (table[i] != 10 * (i + 1)) e |= 16;
    if (ops[0] (3, 4) != 3 + 1 + 4) e |= 32;      /* add: helper(3)+4 */
    if (ops[1] (3, 4) != 12) e |= 64;             /* 159lib_a.c's mul */
    if (twice (21) != 42) e |= 128;
    if (calls != 1000) e |= 256;
    test_errors = e;
}
