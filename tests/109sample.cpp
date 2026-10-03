// *****************************************************************************
//  tests/109sample.cpp — file-scope objects are constructed before main()
//
//  A global of class type used to get no constructor call at all: C has
//  nowhere to put one, so the object began as whatever was in memory.
//  main() now starts by constructing every top-level global, in
//  declaration order (lower.c, phase 7b).
//   1. a default constructor;
//   2. constructor arguments (`Counter g_big(40);`);
//   3. an array of objects, element by element;
//   4. a class with only virtual functions gets its vtable pointer;
//   5. a class with no constructor of its own, holding members that
//      have one;
//   6. order: declaration order, all before main()'s first statement;
//   7. a `static` local of class type (hoisted to file scope).
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_order = 0;     // decimal trail of constructor calls, in order

class Counter
{
public:
    int value;
    Counter()      { value = 7;  g_order = g_order * 10 + 1; }
    Counter(int v) { value = v;  g_order = g_order * 10 + 2; }
};

class Shape
{
public:
    virtual int sides() { return 4; }
};

class Pair
{
public:
    Counter first;
    Counter second;
};

Counter g_plain;            // 1
Counter g_big(40);          // 2
Counter g_row[2];           // 1 1
Shape   g_shape;
Pair    g_pair;             // 1 1

int bump()
{
    static Counter calls(100);
    calls.value++;
    return calls.value;
}

int main()
{
    int errors = 0;

    if (g_plain.value != 7) errors++;
    if (g_big.value != 40) errors++;
    if (g_row[0].value != 7 || g_row[1].value != 7) errors++;
    if (g_shape.sides() != 4) errors++;
    if (g_pair.first.value != 7 || g_pair.second.value != 7) errors++;

    // everything above ran before this line, in declaration order; the
    // static local (hoisted behind the others) comes last
    if (g_order != 1211112) errors++;

    if (bump() != 101 || bump() != 102) errors++;

    test_errors = errors;
    return 0;
}
