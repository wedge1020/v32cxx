/* *****************************************************************************
 *  tests/117sample.c -- C input: calls of variadic functions in the places
 *  where "run the argument assignments first" needs care (cmode.c):
 *
 *   1. the right-hand side of && and ||: its arguments must be evaluated
 *      only when that side is;
 *   2. a while and a for condition: evaluated again every time round,
 *      `continue` included;
 *   3. between the case labels of a switch, and as the unbraced body of an
 *      if / else;
 *   4. nested: a variadic call as an argument of another;
 *   5. a variable declaration's initializer and a return value;
 *   6. struct, union and enum definitions inside other declarations, and a
 *      tag that is also the name of a variable.
 *
 *  Self-checking: test_errors must end at 0. Also valid for a native C
 *  compiler (gcc -x c tests/117sample.c && ./a.out prints the count).
 * ****************************************************************************/

#include <stdarg.h>
#ifndef __V32CXX__
#include <stdio.h>
#endif

int test_errors = -1;

int evaluations = 0;

/* counts how often it runs; returns its argument */
int
touch(int value)
{
    evaluations++;
    return value;
}

int
sum(int n, ...)
{
    va_list ap;
    int total = 0;

    va_start(ap, n);
    while (n-- > 0)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

int
twice(int n)
{
    return sum(2, n, n);
}

int
classify(int n)
{
    switch (n)
    {
        case 1:
            return sum(1, 10);
        case 2:
            n = sum(2, 10, 10);
            break;
        default:
            n = sum(3, 10, 10, 10) + sum(1, n);
    }
    return n;
}

/* ---- 6 ---------------------------------------------------------------------- */
struct point { int x, y; } origin = { 3, 4 }, corners[2] = { { 1, 2 }, { 5, 6 } };

struct shape {
    int kind;
    union {
        struct { int w, h; } box;
        struct { int r; } disc;
    } u;
    enum { SMALL, LARGE = 5 } size;
} shape;        /* (Vircon32 C cannot brace-initialize a union) */

int point = 9;                      /* an ordinary name, same as the tag */

int
main(void)
{
    int e = 0;
    int i, n;
    int total = sum(3, 1, 2, 3);                        /* 5 */
    struct point p;

    if (total != 6) e++;
    if (twice(21) != 42) e++;

    /* 1 */
    evaluations = 0;
    if (touch(0) && sum(2, touch(1), touch(2)) == 3) e++;
    if (evaluations != 1) e++;
    evaluations = 0;
    if (!(touch(1) && sum(2, touch(1), touch(2)) == 3)) e++;
    if (evaluations != 3) e++;
    evaluations = 0;
    if (!(touch(1) || sum(2, touch(1), touch(2)) == 3)) e++;
    if (evaluations != 1) e++;
    evaluations = 0;
    if (touch(0) || sum(2, touch(1), touch(2)) != 3) e++;
    if (evaluations != 3) e++;
    evaluations = 0;
    n = touch(1) && sum(1, touch(5)) == 5 && sum(1, touch(6)) == 6;
    if (n != 1 || evaluations != 3) e++;

    /* 2 */
    i = 0;
    evaluations = 0;
    while (sum(2, touch(i), 1) < 4)
    {
        i++;
        if (i == 1)
            continue;
    }
    if (i != 3 || evaluations != 4) e++;
    n = 0;
    for (i = sum(1, 0); sum(2, i, i) < 6; i++)
        n += i;
    if (i != 3 || n != 3) e++;

    /* 3 */
    if (classify(1) != 10 || classify(2) != 20 || classify(7) != 37) e++;
    if (total == 6)
        n = sum(2, 20, 2);
    else
        n = sum(1, 0);
    if (n != 22) e++;

    /* 4 */
    if (sum(3, sum(1, 1), sum(2, 2, 3), sum(0)) != 6) e++;

    /* 6 */
    p = corners[1];
    if (origin.x + origin.y != 7 || p.x != 5 || p.y != 6) e++;
    shape.kind = 1;
    shape.u.box.w = 7;
    shape.u.box.h = 8;
    shape.size = LARGE;
    if (shape.u.box.w != 7 || shape.u.box.h != 8 || shape.u.disc.r != 7) e++;
    if (shape.size != LARGE || SMALL != 0 || point != 9) e++;

    test_errors = e;
#ifndef __V32CXX__
    printf("test_errors = %d\n", test_errors);
#endif
    return 0;
}
