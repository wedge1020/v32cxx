// *****************************************************************************
//  tests/200sample.cpp -- scoped enumerations and same-named enums in namespaces.
//
//  `enum class Color { Red, Green };` is reached as Color::Red only (the C enum
//  has the enumerators `Color__Red`, `ns__Color__Red` inside a namespace); an
//  underlying type (`: int`, `: char`) is accepted and ignored. An enum TYPE in
//  a namespace is `a__E` in the output, so `a::E` and `b::E` are two types, also
//  in the mangled names of the functions that take them.
//
//  test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

enum class Color { Red, Green = 5, Blue };
enum class Small : int { A, B };
enum Plain { Red, Green };                 // an unscoped one beside them: Red is Plain's

namespace ns { enum class Mode : char { Off, On }; enum Level { Low = 3, High }; }
namespace a { enum E { P, Q }; enum class K { U, V }; }
namespace b { enum E { P = 10, Q = 20 }; enum class K { U = 7, V }; }

struct Holder { a::E ae; b::E be; Color c; ns::Mode m; };

int f(Color c)
{
    switch (c) {
    case Color::Red: return 1;
    case Color::Green: return 2;
    default: return 3;
    }
}
int f(int v) { return 100 + v; }
int f(ns::Mode m) { return m == ns::Mode::On ? 10 : 20; }
int g(a::E x) { return x == a::P ? 1 : 2; }
int g(b::E x) { return x == b::Q ? 3 : 4; }
Color next(Color c) { return c == Color::Red ? Color::Green : Color::Blue; }
int dflt(Color c = Color::Green) { return static_cast<int>(c); }

namespace a { int h(K k) { return k == K::V ? 5 : 6; } E flip(E e) { return e == P ? Q : P; } }
namespace b { int h(K k) { return k == K::V ? 50 : 60; } }

int main(void)
{
    int e = 0;

    Color c = Color::Blue;
    if (static_cast<int>(c) != 6) e++;
    if (static_cast<int>(Color::Red) != 0 || (int)Color::Green != 5) e++;
    if (f(c) != 3 || f(Color::Red) != 1 || f(Color::Green) != 2) e++;
    if (f(7) != 107) e++;                                  // the int overload is another function
    if (f(ns::Mode::On) != 10 || f(ns::Mode::Off) != 20) e++;
    if (next(Color::Red) != Color::Green || next(Color::Green) != Color::Blue) e++;
    if (dflt() != 5 || dflt(Color::Blue) != 6) e++;
    Small s = Small::B;
    if (static_cast<int>(s) != 1 || s == Small::A) e++;
    if (Red != 0 || Green != 1) e++;                        // Plain's, unscoped
    Plain p = Green;
    if (p != Green) e++;

    if (ns::Low != 3 || ns::High != 4) e++;
    ns::Level lv = ns::High;
    if (lv != ns::Level::High) e++;

    // two enums called E, two classes of enums called K
    a::E ae = a::Q;
    b::E be = b::Q;
    if (g(ae) != 2 || g(be) != 3 || g(a::P) != 1 || g(b::P) != 4) e++;
    if (a::P != 0 || a::Q != 1 || b::P != 10 || b::Q != 20) e++;
    if (a::h(a::K::V) != 5 || a::h(a::K::U) != 6 || b::h(b::K::V) != 50) e++;
    if (static_cast<int>(a::K::V) != 1 || static_cast<int>(b::K::U) != 7 || static_cast<int>(b::K::V) != 8) e++;
    if (a::flip(a::P) != a::Q || a::flip(ae) != a::P) e++;
    if (sizeof(a::E) != sizeof(b::E)) e++;

    Holder h;
    h.ae = a::Q; h.be = b::P; h.c = Color::Green; h.m = ns::Mode::On;
    if (h.ae != a::Q || h.be != b::P || h.c != Color::Green || h.m != ns::Mode::On) e++;
    Color arr[3] = { Color::Blue, Color::Red, Color::Green };
    int sum = 0;
    for (int i = 0; i < 3; i++) sum = sum + (int)arr[i];
    if (sum != 11) e++;
    if (arr[1] >= arr[0] || !(arr[0] > arr[2])) e++;

    test_errors = e;
    return 0;
}
