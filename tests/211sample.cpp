// Out-of-line operators of a namespaced class whose first parameter (or conversion type) is a bare name of a
// type of that namespace: `a::C a::C::operator-(D d)`. The token after `operator` used to be looked up in the
// class scope still armed by `C::`, so `D` was no type there and the parameter list a syntax error.
namespace a {
    class D { public: int v; D() : v(0) {} D(int x) : v(x) {} };
    enum Mode { Plain, Twice };
    struct P { int x; int y; };
    namespace in { class K { public: int k; K() : k(0) {} K(int q) : k(q) {} }; }
    class C {
    public:
        int x;
        C() : x(0) {}
        C(int q) : x(q) {}
        C operator-(D d);
        C operator+(D d) const;
        C operator*(Mode m);
        C operator/(P p);
        C operator*(in::K k);
        C operator-();
        C operator!();
        C operator++();
        C operator++(int);
        bool operator==(D d);
        bool operator!=(D d);
        bool operator<(D d);
        int operator()(D d);
        int operator()(D d, D e);
        int operator[](D d);
        C &operator+=(D d);
        C &operator-=(D d);
        C &operator=(D d);
        operator D();
        operator Mode();
        operator int();
        int sub(D d);
    };
    namespace in {
        class Q { public: int q; Q() : q(0) {} Q(int z) : q(z) {} Q operator-(K k); int operator()(K k); operator K(); };
    }
}
a::C a::C::operator-(D d) { return C(x - d.v); }
a::C a::C::operator+(D d) const { return C(x + d.v); }
a::C a::C::operator*(Mode m) { return C(m == Twice ? x * 2 : x); }
a::C a::C::operator/(P p) { return C(x / (p.x + p.y)); }
a::C a::C::operator*(in::K k) { return C(x * k.k); }
a::C a::C::operator-() { return C(-x); }
a::C a::C::operator!() { return C(x == 0 ? 1 : 0); }
a::C a::C::operator++() { x += 1; return *this; }
a::C a::C::operator++(int) { C old(x); x += 1; return old; }
bool a::C::operator==(D d) { return x == d.v; }
bool a::C::operator!=(D d) { return x != d.v; }
bool a::C::operator<(D d) { return x < d.v; }
int a::C::operator()(D d) { return x + d.v; }
int a::C::operator()(D d, D e) { return x + d.v * e.v; }
int a::C::operator[](D d) { return x * d.v; }
a::C &a::C::operator+=(D d) { x += d.v; return *this; }
a::C &a::C::operator-=(D d) { x -= d.v; return *this; }
a::C &a::C::operator=(D d) { x = d.v; return *this; }
a::C::operator a::D() { return D(x); }
a::C::operator Mode() { return x > 5 ? Twice : Plain; }
a::C::operator int() { return x + 1000; }
int a::C::sub(D d) { return x - d.v; }
a::in::Q a::in::Q::operator-(K k) { return Q(q - k.k); }
int a::in::Q::operator()(K k) { return q * k.k; }
a::in::Q::operator K() { return K(q); }

int test_errors = -1;
int main() {
    int e = 0;
    a::D d(3), d2(4);
    a::C c(10);
    a::C r = c - d;               if (r.x != 7) e++;
    r = c + d;                    if (r.x != 13) e++;
    r = c * a::Twice;             if (r.x != 20) e++;
    a::P p; p.x = 2; p.y = 3;
    r = c / p;                    if (r.x != 2) e++;
    a::in::K k(4);
    r = c * k;                    if (r.x != 40) e++;
    r = -c;                       if (r.x != -10) e++;
    r = !c;                       if (r.x != 0) e++;
    a::D d10(10);
    if (!(c == d10)) e++;
    if (c != d10) e++;
    if (!(c < d2) == 1) { }
    if (c < d) e++;
    if (c(d) != 13) e++;
    if (c(d, d2) != 22) e++;
    if (c[d] != 30) e++;
    c += d;                       if (c.x != 13) e++;
    c -= d2;                      if (c.x != 9) e++;
    c = d;                        if (c.x != 3) e++;
    a::C c2(1);
    ++c2;                         if (c2.x != 2) e++;
    r = c2++;                     if (r.x != 2 || c2.x != 3) e++;
    a::C c9(9), c5(5);
    a::Mode m = c9;               if (m != a::Twice) e++;
    int n = c5;                   if (n != 1005) e++;
    if (c2.sub(d) != 0) e++;
    a::in::Q q(9);
    a::in::Q q2 = q - k;          if (q2.q != 5) e++;
    if (q(k) != 36) e++;
    test_errors = e;
    return 0;
}
