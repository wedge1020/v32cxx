// A leading `::` (the global scope qualifier): `::x`, `::f()`, `::a::D`, `::a::f()` in expressions and types.
// It reaches the file-scope name where a namespace declares one of its own (`a::x` is `a__x` in the output), where
// a local or a parameter hides it, and where a namespace function of the same name would be called.
int x = 100;
int arr[3] = {1, 2, 3};
float scale = 1.5f;
int f(int v) { return v + 1; }
int f(float v) { return (int)(v * 2.0f); }
struct G { int a; int b; };
G gv;
enum Color { Red, Green };
enum class Dir { Up, Down };
int (*hook)(int) = 0;
int gi = 3;
int counter(int k) { return 7 + k; }

namespace a {
    int x = 10;
    int arr[3] = {10, 20, 30};
    float scale = 0.5f;
    float gi = 2.5f;                         // another type: `::gi` picks the int overload of f
    int f(int v) { return v + 1000; }
    class D { public: int v; D() : v(5) {} D(int q) : v(q) {} int get() { return v; } };
    int g2(int v) { return v * 3; }
    namespace in {
        int x = 20;
        int f(int v) { return v + 2000; }
        class E { public: int w; E() : w(9) {} };
    }
    int inside_a() {
        int r = 0;
        r += x;                              // a::x: 10
        r += ::x;                            // 100
        r += ::arr[1];                       // 2
        r += arr[1];                         // 20
        r += f(1);                           // 1001
        r += ::f(1);                         // 2
        r += ::a::f(1);                      // 1001
        r += ::a::in::f(1);                  // 2001
        r += in::f(1);                       // 2001
        return r;                            // 10 + 100 + 2 + 20 + 1001 + 2 + 1001 + 2001 + 2001 = 6138
    }
    int hidden_by_local() {
        int x = 3;                           // hides a::x and ::x
        return x + ::x + a::x + ::a::x;      // 3 + 100 + 10 + 10 = 123
    }
    int overload_pick() { return ::f(::gi) * 10 + ::f(gi); }      // f(int) 4 -> 40, f(float) 5 -> 45
    int by_param(int x) { return x + ::x; }
    void set_global(int v) { ::x = v; ::scale = 2.5f; ::x += 1; ::x++; }
    int pointer_to(int k) { int *p = &::x; *p += k; return ::x; }
    ::Color pick() { return ::Green; }
    ::Dir pick2() { return ::Dir::Down; }
    int call_through() { ::hook = ::counter; return ::hook(0) + ::counter(0); }
    int types() {
        ::a::D d(4);
        ::a::in::E e;
        a::D d2(6);
        ::G g;
        g.a = 8; g.b = 9;
        ::G *pg = &::gv;
        pg->a = 40;
        return d.get() + e.w + d2.get() + g.a + g.b + ::gv.a;       // 4 + 9 + 6 + 8 + 9 + 40 = 76
    }
}
class GBase { public: int v; GBase() : v(3) {} virtual int get() { return v; } };
namespace a { class DD : public ::GBase { public: int get() { return v + 1; } }; }
::a::D make_d() { ::a::D d(77); return d; }
int test_errors = -1;
int main() {
    int e = 0;
    if (a::inside_a() != 6138) e++;
    if (a::hidden_by_local() != 123) e++;
    if (a::by_param(5) != 105) e++;
    if (a::overload_pick() != 45) e++;
    a::set_global(50);
    if (x != 52 || scale != 2.5f) e++;           // 50, then += 1, then ++
    if (a::x != 10 || a::scale != 0.5f) e++;
    if (a::pointer_to(8) != 60 || x != 60) e++;
    if (a::pick() != Green) e++;
    if (a::pick2() != Dir::Down) e++;
    if (a::call_through() != 14) e++;
    if (hook == 0) e++;
    if (a::types() != 76) e++;
    if (gv.a != 40) e++;
    ::GBase *gb = new ::a::DD();
    if (gb->get() != 4) e++;
    delete gb;
    ::a::D d = make_d();
    if (d.get() != 77) e++;
    if (::x != 60 || ::arr[2] != 3 || ::f(2) != 3 || ::f(2.0f) != 4) e++;
    if (::a::f(2) != 1002 || ::a::in::x != 20) e++;
    x = ::x + ::a::x;                            // 70
    if (x != 70) e++;
    test_errors = e;
    return 0;
}
