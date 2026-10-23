/* tests/163sample.c -- linking: the macros of one file are not the macros of
 * another. Each file defines N differently, and 163other.c checks for
 * ONLY_HERE, which only this file defines. Self-checking. */
#define N 10
#define ONLY_HERE 1
int test_errors = -1;
int other_n (void);
int other_sees_only_here (void);
static int here_n (void) { return N; }

void main (void)
{
    int e = 0;
    if (here_n () != 10) e |= 1;
    if (other_n () != 20) e |= 2;
    if (other_sees_only_here () != 0) e |= 4;
    test_errors = e;
}
