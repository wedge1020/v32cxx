// *****************************************************************************
//  tests/175sample.cpp — list-initialization of variables: `T x{...}`
//
//  `Counter c{1};` and `Counter c = {1};` used to be emitted as a C brace list
//  (`Counter c = {1};`): the constructor never ran, but the destructor did, and
//  the live count went negative. A class that declares a constructor is now
//  built exactly as `Counter c(1);` is; `{}` is default construction. A scalar
//  takes its single value (`int k{3}` is `int k = 3`, `int k{}` is 0), and an
//  aggregate (no constructor) keeps its C brace initializer.
//
//  test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_live = 0;
int g_built = 0;

class Counter {
public:
    int id;
    Counter() { id = 9; g_live = g_live + 1; g_built = g_built + 1; }
    Counter(int i) { id = i * 10; g_live = g_live + 1; g_built = g_built + 1; }
    Counter(int i, int j) { id = i + j; g_live = g_live + 1; g_built = g_built + 1; }
    ~Counter() { g_live = g_live - 1; }
};

enum Shade { SHADE_LIGHT, SHADE_DARK };
typedef int score_t;

struct P { int x; int y; };

// a global: built at the top of main(), like `Counter g_paren(5);`
Counter g_brace{5};
Counter g_paren(6);
int g_scalar{7};

int main(void)
{
    int e = 0;

    if (g_brace.id != 50) e++;
    if (g_paren.id != 60) e++;
    if (g_scalar != 7) e++;
    int base = g_live;               // the two globals are alive

    {
        Counter c{1};
        Counter d = {2};
        Counter f{3, 4};
        Counter g = {5, 6};
        Counter h{};
        Counter i = {};
        Counter j;
        Counter a{1}, b{2};
        if (c.id != 10) e++;
        if (d.id != 20) e++;
        if (f.id != 7) e++;
        if (g.id != 11) e++;
        if (h.id != 9) e++;
        if (i.id != 9) e++;
        if (j.id != 9) e++;
        if (a.id != 10 || b.id != 20) e++;
        if (g_live != base + 9) e++;
    }
    if (g_live != base) e++;         // every one of them was destroyed
    if (g_built != 2 + 9) e++;       // and every one of them was constructed

    // for-init declaration
    for (Counter k{8}; k.id < 100; k.id = k.id + 100) {
        if (k.id != 80) e++;
        if (g_live != base + 1) e++;
    }
    if (g_live != base) e++;

    // scalars
    int s1{3};
    int s2 = {4};
    int s3{};
    int s4 = {};
    float s5{2};
    int *s6{0};
    if (s1 != 3 || s2 != 4 || s3 != 0 || s4 != 0) e++;
    if (s5 != 2) e++;
    Shade s7{SHADE_DARK};
    score_t s8{9};
    if (s7 != SHADE_DARK || s8 != 9) e++;
    if (s6 != 0) e++;

    // an aggregate keeps its brace list
    P p{1, 2};
    P q = {3, 4};
    P z{};
    if (p.x != 1 || p.y != 2 || q.x != 3 || q.y != 4) e++;
    if (z.x != 0 || z.y != 0) e++;

    test_errors = e;
    return 0;
}
