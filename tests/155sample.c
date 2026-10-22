/* tests/155sample.c -- a ternary whose branches are pointers or arrays
 * The temporary that stands in for the `?:` must have the type the result
 * has after array-to-pointer decay (char *), not the array's own type, not
 * int for pointer arithmetic. Self-checking: test_errors must end at 0. */
int test_errors = -1;

static int first (char *p, int n) { return p[0] + n; }
static int firstc (const char *p, int n) { return p[0] + n; }

void main (void)
{
    char a[8];
    char b[8];
    char *old = a;
    int ol = 1, od = 0, i = 1, e = 0;

    a[0] = 1; a[1] = 2; b[0] = 3; b[1] = 4;
    if (first (i < 0 ? a + 1 : b, i) != 4) e += 1;
    if (first (i > 0 ? a + 1 : b, 0) != 2) e += 2;
    if (firstc (i < 0 ? old + ol : old + od + 1, 0) != 2) e += 4;
    if (firstc (i ? "x" : "y", 0) != 'x') e += 8;
    test_errors = e;
}
