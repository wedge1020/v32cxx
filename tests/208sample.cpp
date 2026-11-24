// A function declared with one spelling of a parameter type and defined with another gets one C name:
// `a::D`, `D` (inside the namespace, by `using a::D` or `using namespace a`), a typedef of either.
// The prototype and the definition must agree for free functions, methods, constructors and operators, or
// the program calls a function the output never defines.
namespace a {
    struct D { int v; };
    typedef D DT;
    int f(D d);
    int g(D d);
    int h(D *d);
    int k(const D &d);
    int w(DT d);
    int lf(float x);
    int lf(D d);
    struct C {
        int base;
        C(D d);
        int m(D d);
        int n(DT d);
        int operator+(D d);
        int operator-(D d);
    };
    namespace in {
        struct E { int w; };
        int p(E e);
        int p2(a::D d);
    }
}
namespace b {
    using a::D;
    int q(D d);
    int q2(a::D d);
}
namespace c {
    int r(a::D d);
    int r2(a::DT d);
    int r3(const a::D *d);
}
typedef a::D AD;
using namespace a;

int a::f(a::D d) { return d.v + 1; }
int a::g(DT d) { return d.v + 2; }
int a::h(AD *d) { return d->v + 3; }
int a::k(const a::D &d) { return d.v + 4; }
int a::w(a::D d) { return d.v + 5; }
int a::lf(float x) { return (int)x; }
int a::lf(a::D d) { return d.v + 6; }
int a::in::p(a::in::E e) { return e.w + 7; }
int a::in::p2(D d) { return d.v + 8; }
int b::q(a::D d) { return d.v + 9; }
int b::q2(D d) { return d.v + 10; }
int c::r(D d) { return d.v + 11; }
int c::r2(AD d) { return d.v + 12; }
int c::r3(const D *d) { return d->v + 13; }
a::C::C(a::D d) { base = d.v + 14; }
int a::C::m(a::D d) { return base + d.v + 15; }
int a::C::n(AD d) { return base + d.v + 16; }
int a::C::operator+(a::D d) { return base + d.v + 17; }
int a::C::operator-(a::DT d) { return base + d.v + 18; }

int test_errors = -1;
int main() {
    int e = 0;
    a::D d; d.v = 100;
    a::in::E ie; ie.w = 200;
    if (a::f(d) != 101) e++;
    if (a::g(d) != 102) e++;
    if (a::h(&d) != 103) e++;
    if (a::k(d) != 104) e++;
    if (a::w(d) != 105) e++;
    if (a::lf(2.0f) != 2) e++;
    if (a::lf(d) != 106) e++;
    if (a::in::p(ie) != 207) e++;
    if (a::in::p2(d) != 108) e++;
    if (b::q(d) != 109) e++;
    if (b::q2(d) != 110) e++;
    if (c::r(d) != 111) e++;
    if (c::r2(d) != 112) e++;
    if (c::r3(&d) != 113) e++;
    a::C o(d);
    if (o.base != 114) e++;
    if (o.m(d) != 114 + 100 + 15) e++;
    if (o.n(d) != 114 + 100 + 16) e++;
    if ((o + d) != 114 + 100 + 17) e++;
    if ((o - d) != 114 + 100 + 18) e++;
    // the same calls spelled the other ways
    { using a::f; if (f(d) != 101) e++; }
    if (f(d) != 101) e++;
    test_errors = e;
    return 0;
}
