/* tests/158bad.c -- a failing _Static_assert stops the transpile, with its message (G5) */
_Static_assert (1 + 1 == 3, "arithmetic is broken");
void main (void) { }
