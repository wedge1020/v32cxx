// *****************************************************************************
//  tests/120sample.cpp — function objects: calls through operator()
//
//  `f(5)` on an object whose class declares operator() means
//  `f.operator()(5)`. Covers overloads, a reference parameter, a member
//  object called from a method and from outside, a global, a virtual
//  operator() reached through `(*p)(x)`, and arrays of function objects.
//  (Before 20261009 the call was left as `f(5)` in the generated C, which
//  the Vircon32 compiler rejects: "indirect call callee must be a function
//  pointer".)
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
class Adder { public: int base; Adder(int b) { base = b; } int operator()(int x) { return base + x; } int operator()(int x, int y) { return base + x + y; } };
class Scaler { public: virtual int operator()(int x) { return x; } };
class Doubler : public Scaler { public: int operator()(int x) { return x * 2; } };
class Holder { public: Adder add; Holder() : add(100) {} int run(int v) { return add(v); } };
int apply(Adder &a, int v) { return a(v); }
Adder g_add(1000);
int main()
{
    int e = 0;
    Adder f(10);
    if (f(5) != 15) e++;
    if (f(1, 2) != 13) e++;
    if (apply(f, 7) != 17) e++;
    Holder h;
    if (h.run(1) != 101) e++;
    if (h.add(2) != 102) e++;
    Doubler d;
    Scaler *s = &d;
    if ((*s)(21) != 42) e++;
    if (g_add(1) != 1001) e++;
    Adder arr[2] = { Adder(1), Adder(2) };
    if (arr[0](1) + arr[1](1) != 5) e++;
    test_errors = e;
    return 0;
}
