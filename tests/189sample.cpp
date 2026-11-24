// *****************************************************************************
//  tests/189sample.cpp -- function pointers initialised at file / namespace scope
//
//  A C++ function's name in the generated C carries its signature
//  (`square__int`), so `int (*h)(int) = square;` must be rewritten there as
//  well as in a function body: the mangled name, with the explicit `&` the
//  Vircon32 compiler wants. Covered: bare name, `&name`, a namespace's own
//  (unqualified, qualified, from outside), a typedef'd pointer, arrays of
//  pointers with brace lists (file scope, namespace, local), a pointer
//  returning a class pointer, and overloaded names picked by the pointer's
//  own type. Everything is called through the pointers. Self-checking.
// *****************************************************************************

int test_errors = -1;

class D {
public:
    int v;
    D() { v = 5; }
};

D *make() { return new D(); }
int square(int x) { return x * x; }
int inc(int x) { return x + 1; }
int dec(int x) { return x - 1; }
int over(int x) { return x + 100; }
int over(float x) { return 7; }
int sum2(int a, int b) { return a + b; }

typedef int (*Fn)(int);

D *(*g)() = make;
int (*h)(int) = &square;
int (*ops[2])(int) = { inc, dec };
static int (*sops[3])(int) = { &square, inc, &dec };
Fn gfn = square;
Fn tab[2] = { &inc, dec };
int (*ho_int)(int) = over;                // the overload taking an int
int (*ho_float)(float) = over;            // ... and the one taking a float
int (*two)(int, int) = sum2;
int (*unset)(int) = nullptr;

namespace n {
    int tw(int x) { return 2 * x; }
    int (*hp)(int) = tw;                  // unqualified, inside the namespace
    int (*hq)(int) = &tw;
    int (*hr)(int) = n::tw;               // qualified
    int (*tabn[2])(int) = { tw, square }; // a namespace function and a file-scope one
    namespace deep {
        int three(int x) { return 3 * x; }
        int (*hd)(int) = three;
    }
}
int (*hs)(int) = n::tw;                   // from outside
int (*hd2)(int) = n::deep::three;

int main(void)
{
    int e = 0;

    D *d = g();
    if (d == 0 || d->v != 5) e++;
    delete d;
    if (h(7) != 49 || gfn(6) != 36) e++;
    if (ops[0](10) != 11 || ops[1](10) != 9) e++;
    if (sops[0](4) != 16 || sops[1](4) != 5 || sops[2](4) != 3) e++;
    if (tab[0](1) != 2 || tab[1](1) != 0) e++;
    if (ho_int(1) != 101 || ho_float(1.0f) != 7) e++;
    if (two(3, 4) != 7) e++;
    if (unset != 0) e++;
    if (n::hp(5) != 10 || n::hq(5) != 10 || n::hr(5) != 10) e++;
    if (n::tabn[0](3) != 6 || n::tabn[1](3) != 9) e++;
    if (n::deep::hd(2) != 6 || hs(4) != 8 || hd2(4) != 12) e++;

    // reassigned later, and the locals that always worked
    h = dec;
    if (h(7) != 6) e++;
    ho_float = over;                       // an overload, by the pointer's type
    if (ho_float(2.0f) != 7) e++;
    int (*loc)(int) = square;
    int (*lops[2])(int) = { inc, n::tw };
    if (loc(3) != 9 || lops[0](3) != 4 || lops[1](3) != 6) e++;

    test_errors = e;
    return 0;
}
