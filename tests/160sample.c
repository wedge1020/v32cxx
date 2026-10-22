/* tests/160sample.c -- a library (gap G6): `v32c --mklib lib160.v32a
 * 160lib/*.c` makes an index; linking with it takes in only the members
 * the program needs (add.c, and mul.c with deep.c, which mul3 calls) and
 * leaves unused.c out. Self-checking: test_errors must end at 0. */
#include "160lib/shared.h"
int test_errors = -1;

void main (void)
{
    int e = 0;
    if (add (1, 2) != 4) e |= 1;
    if (mul3 (1) != 303) e |= 2;
    if (lib_counter != 10) e |= 4;
    test_errors = e;
}
