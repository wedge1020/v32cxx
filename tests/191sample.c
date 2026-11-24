/* *****************************************************************************
 *  tests/191sample.c -- C's `auto` storage class, and `extern` inside a function
 *
 *  - `auto char buf[8];` / `auto int n = 3;` / `auto STAT s;`: in C, `auto` is
 *    the default storage class, accepted and dropped (C++ input keeps C++11
 *    type deduction; sample 19 and friends cover that).
 *  - `extern char **environ;` inside a function, with no declaration anywhere
 *    else (Rogue's save.c): the declaration IS the variable, like a file-scope
 *    `extern` nothing defines, so it is hoisted to file scope. `extern int
 *    seed;` in two functions, with the program's own `int seed = 5;` further
 *    down: one variable, the definition.
 *  Self-checking: test_errors must end at 0. The Makefile checks the output.
 * ***************************************************************************** */

typedef struct { int a; int b; } STAT;

int test_errors = -1;

int fn_one(void)
{
    extern int seed;
    return seed + 1;
}

int fn_two(void)
{
    extern int seed;
    extern char **environ;
    char *p;
    auto char buf[8];
    auto int n = 3;
    auto STAT s;

    p = buf;
    environ = &p;
    buf[0] = 'x';
    buf[1] = 0;
    s.a = n;
    s.b = buf[0];
    return seed + s.a + (s.b == 'x') + (*environ == buf);
}

int seed = 5;

int main(void)
{
    int e = 0;
    if (fn_one() != 6) e += 1;
    if (fn_two() != 5 + 3 + 1 + 1) e += 2;
    test_errors = e;
    return 0;
}
