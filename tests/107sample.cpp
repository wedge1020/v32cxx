// *****************************************************************************
//  tests/107sample.cpp — unnamed objects: `Enemy(1, 2, 3)` as an expression
//
//  A class name used like a function builds an object with no name. Each
//  one becomes an ordinary local, declared just ahead of the statement
//  that uses it and inside a block of its own, so it is constructed
//  before the statement and destroyed right after it (ast.c,
//  desugar_unnamed_objects).
//   1. as an argument to a `const T &` parameter and to a by-value one;
//   2. as a method's receiver, and nested inside another unnamed object;
//   3. `T x = T(args);` builds x directly: one constructor call, no copy;
//   4. assigned to an existing object, and returned from a function;
//   5. with no arguments (the default constructor);
//   6. as the single statement of an `if`/`else`/loop body, and in an
//      `if` condition;
//   7. lifetime: destroyed at the end of its statement;
//   8. a typedef name used the same way is a cast.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_ctors = 0;
int g_dtors = 0;

typedef int Whole;

class Vec
{
public:
    int x;
    int y;
    Vec()             { x = 0; y = 0; g_ctors++; }
    Vec(int a, int b) { x = a; y = b; g_ctors++; }
    ~Vec()            { g_dtors++; }
    int sum() const   { return x + y; }
};

class Line
{
public:
    Vec from;
    Vec to;
    Line(const Vec &a, const Vec &b) { from.x = a.x; from.y = a.y; to.x = b.x; to.y = b.y; }
    int length2() const { return (to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y); }
};

int sumOf(const Vec &v)       { return v.x + v.y; }
int sumByValue(Vec v)         { return v.x + v.y; }
Vec make(int a)               { return Vec(a, a * 2); }

int main()
{
    int errors = 0;

    // arguments
    if (sumOf(Vec(1, 7)) != 8) errors++;
    if (sumByValue(Vec(2, 3)) != 5) errors++;
    if (sumOf(Vec()) != 0) errors++;

    // receiver, and nested
    if (Vec(4, 5).sum() != 9) errors++;
    if (Line(Vec(0, 0), Vec(3, 4)).length2() != 25) errors++;

    // lifetime: everything built so far is already gone
    if (g_ctors != g_dtors) errors++;

    // `T x = T(args);` is one object, built in place
    int before = g_ctors;
    Vec a = Vec(10, 20);
    if (g_ctors != before + 1 || a.sum() != 30) errors++;

    // assignment to an existing object
    a = Vec(1, 2);
    if (a.x != 1 || a.y != 2) errors++;

    // returned from a function
    Vec m = make(5);
    if (m.x != 5 || m.y != 10) errors++;

    // as the only statement of a branch or a loop body; in a condition
    int total = 0;
    if (a.x == 1) total += sumOf(Vec(1, 1));
    else          total += sumOf(Vec(9, 9));
    for (int i = 0; i < 3; i++) total += sumOf(Vec(i, 0));
    if (sumOf(Vec(2, 2)) == 4) total += 100;
    if (total != 105) errors++;

    // a typedef name used the same way is a cast
    float f = 2.9;
    if (Whole(f) != 2) errors++;

    test_errors = errors;
    return 0;
}
