// *****************************************************************************
//  tests/105sample.cpp — member ARRAYS of class objects are constructed
//  and destroyed
//
//  `Counter items[3];` as a class member used to be skipped by both
//  constructor injection and destructor chaining: the elements were never
//  constructed (no constructor body, no vtable pointer) and never
//  destroyed. Local arrays already worked; members did not.
//   1. each element's constructor runs, in index order, before the owning
//      constructor's body;
//   2. each element's destructor runs, last element first, after the
//      owning destructor's body;
//   3. a class with no constructor/destructor of its own still gets both
//      when it holds such an array;
//   4. elements with virtual functions and no constructor get their vtable;
//   5. the length may be a named constant;
//   6. the same through `new`/`delete`.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_ctors = 0;
int g_dtors = 0;
int g_order = 0;     // decimal trail of what ran, in order

enum Limits { SLOTS = 2 };

class Counter
{
public:
    int value;
    int id;
    Counter()  { value = 3; id = g_ctors; g_ctors++; g_order = g_order * 10 + 1; }
    ~Counter() { g_dtors++; g_order = g_order * 10 + 2 + id; }
};

class Shape
{
public:
    virtual int sides() { return 4; }
};

class Box                 // its own constructor and destructor
{
public:
    Counter items[3];
    int     total;
    Box()  { total = items[0].value + items[1].value + items[2].value; g_order = g_order * 10 + 7; }
    ~Box() { g_order = g_order * 10 + 8; }
};

class Plain               // neither: both are implicit
{
public:
    Counter slots[SLOTS];
    Shape   shapes[2];
};

int main()
{
    int errors = 0;

    {
        Box b;
        // three element constructors, then the body
        if (g_ctors != 3 || g_order != 1117) errors++;
        if (b.total != 9) errors++;
        if (b.items[2].id != 2) errors++;
        g_order = 0;
    }
    // the body, then elements 2, 1, 0  (digit = 2 + id)
    if (g_dtors != 3 || g_order != 8432) errors++;

    g_ctors = 0; g_dtors = 0; g_order = 0;
    {
        Plain p;
        if (g_ctors != 2) errors++;
        if (p.slots[1].value != 3) errors++;
        if (p.shapes[0].sides() != 4 || p.shapes[1].sides() != 4) errors++;
    }
    if (g_dtors != 2) errors++;

    g_ctors = 0; g_dtors = 0;
    Box *heap = new Box();
    if (g_ctors != 3 || heap->total != 9) errors++;
    delete heap;
    if (g_dtors != 3) errors++;

    test_errors = errors;
    return 0;
}
