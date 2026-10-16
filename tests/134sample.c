/* *****************************************************************************
 *  tests/134sample.c -- the same conveniences, with C input
 *
 *  // comments, adjacent string literals across lines, a backslash-newline in
 *  a literal and in a macro, and the ternary operator.
 *  Self-checking: test_errors must end at 0.
 * ***************************************************************************** */

#include "string.h"

#define SUM3(a, b, c) \
    ((a) + \
     (b) + \
     (c))

int test_errors = -1;

int pick(int a, int b)
{
    return a > b ? a : b;                  // ternary in a return
}

void main(void)
{
    int e = 0;
    int x = 3;     // comment with ? : and "quotes"
    int y = x > 2 ? 10 : 20;
    char s[] = "one " "two" /* mid */ " three";
    char t[] = "long \
line";

    // a comment that ends with a backslash \
    x = 99;

    if (x != 3) e++;
    if (y != 10) e++;
    if (strlen(s) != 13) e++;
    if (strlen(t) != 9) e++;
    if (SUM3(1, 2, 3) != 6) e++;
    if (pick(4, 9) != 9) e++;

    test_errors = e;
}
