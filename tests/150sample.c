/* *****************************************************************************
 *  tests/150sample.c -- abs() and the SDK's math.h together
 *
 *  The SDK's math.h defines abs(); so did v32libc.c, under the same name, and
 *  the two only coexisted when v32c++ could read math.h (or when the
 *  program was built with a -D MATH_H that then broke math.h itself). The
 *  library's version is now v32_abs(), reached through a macro, so a program
 *  can include math.h (for sin, sqrt...) and v32libc.c in either order, with
 *  the SDK found or not, and call abs() with nothing else to arrange.
 *  Self-checking: test_errors must end at 0. The Makefile also checks the
 *  generated C: math.h still included, no second abs() definition.
 * ***************************************************************************** */

#include "math.h"
#include <v32libc.c>

int test_errors = -1;

void main(void)
{
    int e = 0;
    float s = sin(0.0);

    if (abs(-5) != 5) e += 1;
    if (abs(7) != 7) e += 2;
    if (abs(0) != 0) e += 4;
    if (abs(3 - 10) != 7) e += 8;
    if (s != 0.0) e += 16;

    test_errors = e;
}
