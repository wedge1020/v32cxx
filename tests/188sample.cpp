// *****************************************************************************
//  tests/188sample.cpp -- same-named variables in different namespaces
//
//  A variable declared inside a namespace gets its namespace in its C name,
//  the way a namespace function does: `a::x` is `a__x`, `a::inner::x` is
//  `a__inner__x`; a file-scope variable keeps its name. Before, `a::x`,
//  `b::x` and a file-scope `x` were all the C variable `x`.
//  Every way of naming one must resolve: qualified, unqualified inside the
//  namespace (reopened too, and in a function defined there), after
//  `using namespace` / `using a::x`, nested namespaces, a local hiding it.
//  Objects with constructors and destructors take part: they are built and
//  destroyed (construct_globals_in_main / destroy_globals_in_main) by their
//  C names. Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

int x = 100;                     // file scope: stays `x`
int g_dtors = 0;

class Cell {
public:
    int v;
    Cell() { v = 7; }
    Cell(int n) { v = n; }
    ~Cell() { g_dtors = g_dtors + 1; }
    int get() { return v; }
};

class Other {
public:
    int w;
    Other(int n) { w = n * 10; }
    int get() { return w; }
};

namespace a {
    int x = 1;
    int arr[3] = { 11, 12, 13 };
    Cell c(21);
    int *px = &x;
    int twice() { return x * 2; }                  // unqualified, in a function of a
    namespace inner {
        int x = 3;
        Cell c(23);
        int up() { return x + a::x; }              // inner x, then a::x
        int outer_only() { return arr[1]; }        // found in the enclosing a
    }
}

namespace b {
    int x = 2;
    int arr[3] = { 21, 22, 23 };
    Other c(4);                                    // same name, another class
    int twice() { return x * 2; }
}

namespace a {                                      // reopened
    int y = 5;
    int sum() { return x + y + inner::x; }
    class Probe {
    public:
        int k;
        Probe() { k = 9; }
        int read() { return x + k; }               // a's x from a method
        int shadow(int x) { return x + a::x; }     // parameter hides, a::x names it
        int member_hides();
    };
    int Probe::member_hides() { return k + y; }
}

int Probe_read() { a::Probe p; return p.read(); }

int shadowed()
{
    int r = 0;
    int x = 50;                                    // a local hides every x
    r = x;
    {
        int y = 60;
        r = r + y;
    }
    return r;
}

int through_using()
{
    using namespace a;
    return y + 1;                                  // a::y
}

namespace ns_u {
    int total = 40;
    int bump() { total = total + 1; return total; }
}
using ns_u::total;

int main(void)
{
    int e = 0;

    // qualified, every access path
    if (a::x != 1 || b::x != 2 || x != 100) e++;
    if (a::inner::x != 3 || a::y != 5) e++;
    if (a::arr[2] != 13 || b::arr[2] != 23) e++;
    if (*a::px != 1) e++;
    a::x = 10;
    if (*a::px != 10 || a::x != 10 || b::x != 2) e++;
    a::x += 1;
    a::x++;
    if (a::x != 12) e++;
    a::x = 1;
    int *pb = &b::x;
    *pb = 20;
    if (b::x != 20 || a::x != 1) e++;
    b::x = 2;

    // unqualified, inside the namespaces
    if (a::twice() != 2 || b::twice() != 4) e++;
    if (a::inner::up() != 4 || a::inner::outer_only() != 12) e++;
    if (a::sum() != 1 + 5 + 3) e++;
    if (Probe_read() != 1 + 9) e++;
    a::Probe pr;
    if (pr.shadow(4) != 5 || pr.member_hides() != 9 + 5) e++;

    // locals and using
    if (shadowed() != 110) e++;
    if (through_using() != 6) e++;
    if (total != 40) e++;
    ns_u::bump();
    if (total != 41 || ns_u::total != 41) e++;

    // objects with constructors and destructors, same names, two classes
    if (a::c.get() != 21 || a::inner::c.get() != 23 || b::c.get() != 40) e++;
    a::c.v = 30;
    if (a::c.get() != 30 || a::inner::c.v != 23) e++;
    Cell *pc = &a::inner::c;
    if (pc->get() != 23) e++;

    test_errors = e;
    return 0;
}
