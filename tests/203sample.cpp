// Out-of-line definitions with namespace-qualified names: `int a::f(E e) {...}`, `void a::C::m() {...}`,
// `a::C::C(int x) : ... {}`, `a::C::~C() {}`. They mean what they would mean written inside `namespace a`:
// default arguments come from the declaration, the namespace's names are visible unqualified in the
// parameters and the body, overloads are told apart, and the C names are the namespace's (`a__f`).
enum E { E_A, E_B };
int k = 1000;                    // file scope: hidden by a::k inside the definitions
namespace a {
    int k = 5;
    enum E2 { X1 = 3 };
    int f(E e, int y = 7);
    int f2(E2 e, int y = 1);
    int g(int);
    int g(float);
    class C {
    public:
        int v;
        C(int x);
        ~C();
        void m();
        int get(int d = 2);
        int sum(int a, int b = 4) const;
    };
    namespace b {
        int q = 20;
        enum F { F1 = 4 };
        int h(F f, int z = 1);
        class D {
        public:
            int w;
            D();
            D(int x);
            ~D();
            int s(int t = 3) const;
            D operator+(D o);
        };
    }
}
int a::f(E e, int y) { return e + y + k; }
int a::f2(E2 e, int y) { return e + y + k; }          // E2 is a::E2 here
int a::g(int i) { return i + k; }
int a::g(float f) { return 1000 + k; }
void a::C::m() { v = v + k; }
a::C::C(int x) : v(x) {}
a::C::~C() { k = 99; }
int a::C::get(int d) { return v + d + X1; }
int a::C::sum(int a, int b) const { return v + a + b; }
int a::b::h(F f, int z) { return f + z + q + k; }      // k: a::k, found from a::b
a::b::D::D() : w(1) {}
a::b::D::D(int x) : w(x + q) {}
a::b::D::~D() { q = 77; }
int a::b::D::s(int t) const { return w + t + k + F1; }
a::b::D a::b::D::operator+(a::b::D o) { return D(w + o.w); }
int test_errors = -1;
int main() {
    int e = 0;
    if (a::f(E_B) != 1 + 7 + 5) e++;
    if (a::f(E_B, 0) != 1 + 0 + 5) e++;
    if (a::f2(a::X1) != 3 + 1 + 5) e++;
    if (a::g(1) != 6) e++;
    if (a::g(1.0f) != 1005) e++;
    a::C *c = new a::C(10);
    c->m();
    if (c->v != 15) e++;
    if (c->get() != 15 + 2 + 3) e++;
    if (c->get(0) != 15 + 3) e++;
    if (c->sum(1) != 15 + 1 + 4) e++;
    delete c;
    if (a::k != 99) e++;
    if (k != 1000) e++;
    if (a::b::h(a::b::F1) != 4 + 1 + 20 + 99) e++;
    a::b::D d1;
    a::b::D *d2 = new a::b::D(2);
    if (d1.w != 1) e++;
    if (d2->w != 22) e++;
    if (d2->s() != 22 + 3 + 99 + 4) e++;
    a::b::D d3 = d1 + *d2;
    if (d3.w != 43) e++;   // D(1 + 22) = D(23): w = 23 + q
    delete d2;
    if (a::b::q != 77) e++;
    test_errors = e;
    return 0;
}
