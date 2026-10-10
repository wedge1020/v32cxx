/* *****************************************************************************
 *  tests/128sample.c -- floats through `...` in C
 *
 *  An extra argument travels as one word in an array (see cmode.c). A
 *  float used to be converted to an int on the way in, so va_arg(ap, float)
 *  read back garbage; its bits are stored as they are now. Ints mixed in.
 *  Self-checking: test_errors must end at 0.
 * ***************************************************************************** */

#include <stdarg.h>

int test_errors = -1;

float sumf(int n, ...)
{
    va_list ap;
    float s = 0;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++) s += va_arg(ap, float);
    va_end(ap);
    return s;
}

float mixed(int n, ...)
{
    va_list ap;
    float s = 0;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++) {
        int scale = va_arg(ap, int);
        s += scale * va_arg(ap, float);
    }
    va_end(ap);
    return s;
}

int main(void)
{
    int e = 0;
    float a = 1.5;
    if (sumf(2, a, 2.25) != 3.75) e++;
    if (sumf(0) != 0) e++;
    if (mixed(2, 2, 0.5, 3, a) != 5.5) e++;
    test_errors = e;
    return 0;
}
