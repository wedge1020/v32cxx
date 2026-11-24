// *****************************************************************************
//  tests/193sample.cpp -- enumerators of a namespace's enum are namespace-scope
//  names, like namespace variables: `a::RED` and `b::RED` are two constants,
//  `a__RED` and `b__RED` in the C output (the enum stays a C `enum`). File-scope
//  enumerators keep their names. Every way of writing one is covered:
//  qualified, `a::E::RED`, unqualified inside the namespace (reopened too), in
//  a method, `using namespace`, `using a::RED`, `case` labels, array sizes,
//  default arguments, other enumerators' values, initializers.
//
//  Self-checking: test_errors must end at 0. The Makefile checks the output.
// *****************************************************************************

int test_errors = -1;

enum Top { TOP_A, TOP_B = 40 };          // file scope: unchanged

namespace a {
    enum E { RED, GREEN = 4, BLUE = GREEN + 1 };      // 0, 4, 5
    enum G { SAME = RED + 100, LAST };                // 100, 101 (another enum, same namespace)
    int arr[BLUE];                                    // 5 elements
    int pick(int x = GREEN) {                         // default argument
        switch (x) {                                  // case labels, qualified or not
            case RED: return 10;
            case a::GREEN: return 20;
            case a::E::BLUE: return 30;
            default: return -1;
        }
    }
    struct Pal { int c; int get() { return c + RED + BLUE; } };   // in a method
    namespace in { enum H { RED = 77, DEEP = RED + 1 }; int d = DEEP; }   // a__in__RED; sees its own RED
}

namespace b {
    enum F { RED = 7, GREEN };                        // 7, 8: another RED
    int arr[GREEN];                                   // 8 elements
    int use() { return RED + a::RED + a::BLUE; }      // 7 + 0 + 5
}

namespace a { enum K { MORE = BLUE + 1 }; int back() { return MORE + GREEN; } }   // reopened

int big[a::BLUE + b::RED];                            // 12 elements, qualified
int small[a::E::GREEN];                               // 4 elements, through the enum

namespace shadow {
    enum S { RED = 3 };
    int f(int RED_) { int RED2 = RED + RED_; return RED2; }
    int g() { int RED = 50; return RED; }             // a local hides the enumerator
}

using a::BLUE;                                        // using-declaration of an enumerator
using namespace b;

int main(void)
{
    int e = 0;
    a::E v = a::RED;
    a::E w = a::E::GREEN;
    b::F x = b::RED;
    Top t = TOP_B;

    if (v != 0 || w != 4 || x != 7 || t != 40) e += 1;
    if (a::pick() != 20 || a::pick(a::RED) != 10 || a::pick(a::BLUE) != 30 || a::pick(99) != -1) e += 2;
    if (b::use() != 12) e += 4;
    { a::Pal p; p.c = 1; if (p.get() != 6) e += 8; }
    if (a::in::d != 78 || a::in::RED != 77 || a::RED != 0) e += 16;
    if (a::back() != 10) e += 32;                                  // 6 + 4
    if (sizeof(a::arr) / sizeof(a::arr[0]) != 5 || sizeof(b::arr) / sizeof(int) != 8) e += 64;
    if (sizeof(big) / sizeof(int) != 12 || sizeof(small) / sizeof(int) != 4) e += 128;
    if (shadow::f(1) != 4 || shadow::g() != 50) e += 256;
    if (BLUE != 5 || GREEN != 8 || RED != 7) e += 512;             // using a::BLUE; using namespace b
    if (a::SAME != 100 || a::LAST != 101) e += 1024;
    test_errors = e;
    return 0;
}
