// *****************************************************************************
//  tests/123sample.cpp — reference data members
//
//  `int &r;` in a class: bound by the constructor's initializer list
//  (`: r(x)`), and from then on every use is the referent -- reads,
//  writes, compound assignment, ++, member access and method calls
//  through a class-typed reference member, forwarding to a reference
//  parameter, binding a reference local, returning it by reference, from
//  a const method, through a pointer to the object, and in a copied
//  object (which refers to the same thing). Lowered as a pointer member.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int more_errors();
int g_score = 0;
class Counter { public: int hits; Counter() { hits = 0; } void hit() { hits++; } };
class Ref
{
    public:
        int &r;
        Counter &c;
        float &f;
        Ref(int &x, Counter &k, float &fl) : r(x), c(k), f(fl) {}
        void bump() { r = r + 1; r += 2; c.hit(); f = f * 2.0; }
        int get() const { return r; }
        int *addr() { return &r; }
};
class Global { public: int &s; Global() : s(g_score) {} };
int main()
{
    int e = 0;
    int n = 1;
    float fv = 1.5;
    Counter k;
    Ref a(n, k, fv);
    a.bump();
    if (n != 4 || k.hits != 1 || fv != 3.0) e++;
    a.r = 10;
    if (n != 10 || a.get() != 10) e++;
    int copy = a.r;
    if (copy != 10 || a.addr() != &n) e++;
    Ref *p = &a;
    p->r++;
    p->c.hit();
    if (n != 11 || k.hits != 2) e++;
    Global g;
    g.s = 5;
    if (g_score != 5) e++;
    e += more_errors();
    test_errors = e;
    return 0;
}

void add3(int &v) { v += 3; }
class Box { public: int v; Box() { v = 0; } };
class View
{
    public:
        int &n;
        Box &b;
        View(int &x, Box &bx) : n(x), b(bx) {}
        void fwd() { add3(n); add3(b.v); }
        int &ref() { return n; }
        Box &box() { return b; }
};
int more_errors()
{
    int e = 0;
    int x = 1;
    Box bx;
    View w(x, bx);
    w.fwd();
    if (x != 4 || bx.v != 3) e++;
    int &alias = w.n;
    alias = 9;
    if (x != 9) e++;
    w.ref() = 20;
    if (x != 20) e++;
    w.box().v = 7;
    if (bx.v != 7) e++;
    add3(w.n);
    if (x != 23) e++;
    View copy = w;
    copy.n = 1;
    if (x != 1) e++;
    return e;
}

