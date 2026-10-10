// *****************************************************************************
//  tests/129sample.cpp — several objects constructed in one declaration
//
//  `P a(1), b(2, 3), c(4);` constructs each object from its own arguments
//  (each may pick a different constructor) and destroys all of them at the
//  end of the scope; the same in a loop body, every time round. Only the
//  first object of a declaration could be written this way before.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_dtors = 0;
class P { public: P(int a) : v(a) {} P(int a, int b) : v(a + b) {} ~P() { g_dtors++; } int v; };
int scope()
{
    P a(1), b(2, 3), c(4);
    return a.v + b.v + c.v;
}
int main(void)
{
    int e = 0;
    if (scope() != 10) e++;
    if (g_dtors != 3) e++;
    for (int i = 0; i < 2; i++) { P x(i), y(10); if (x.v + y.v != 10 + i) e++; }
    if (g_dtors != 7) e++;
    test_errors = e;
    return 0;
}
