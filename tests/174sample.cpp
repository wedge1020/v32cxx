// *****************************************************************************
//  tests/174sample.cpp — arrays of objects are built in place
//
//  `Counter a[3] = { Counter(1), Counter(2), Counter(3) };` used to build three
//  named temporaries, copy them into the array (no element constructor ever ran)
//  and destroy BOTH the elements and the temporaries at the end of the scope:
//  seven destructor calls for three constructor calls. C++17 initializes each
//  element directly from its initializer: one constructor and one destructor
//  per element, nothing else, destroyed in the reverse order of construction.
//
//  Every form is run against a log: a constructor logs its id, a destructor the
//  negated id, the default constructor 9 and the copy constructor 100 + id. test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_log[64];
int g_n = 0;
int g_live = 0;

class Counter {
public:
    int id;
    Counter() { id = 9; g_log[g_n] = 9; g_n = g_n + 1; g_live = g_live + 1; }
    Counter(int i) { id = i; g_log[g_n] = i; g_n = g_n + 1; g_live = g_live + 1; }
    Counter(const Counter &o) { id = o.id; g_log[g_n] = 100 + o.id; g_n = g_n + 1; g_live = g_live + 1; }
    ~Counter() { g_log[g_n] = 0 - id; g_n = g_n + 1; g_live = g_live - 1; }
};

// a global array of objects: built, in order, before main() begins (and, as for
// every global here, never destroyed)
Counter g_arr[2] = { Counter(7), { 8 } };

// no copy constructor of its own: an object element is copied member by member
class Plain {
public:
    int v;
    Plain(int x) { v = x; }
};

int e = 0;

// compares the log with the first `n` values given (and that no object is left alive), then clears it
void expect(int n, int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
    int want[8];
    want[0] = a0; want[1] = a1; want[2] = a2; want[3] = a3;
    want[4] = a4; want[5] = a5; want[6] = a6; want[7] = a7;
    if (g_n != n) e++;
    for (int i = 0; i < n && i < 8; i++)
        if (g_log[i] != want[i]) e++;
    if (g_live != 0) e++;
    g_n = 0;
}

int early(int k)
{
    Counter a[2] = { Counter(1), Counter(2) };
    if (k) return 7;
    return 8;
}

int main(void)
{
    // the global array was built by now: 7, then 8 (checked, then the log restarts)
    if (g_n != 2 || g_log[0] != 7 || g_log[1] != 8 || g_arr[0].id != 7 || g_arr[1].id != 8) e++;
    g_n = 0;
    g_live = 0;

    { Counter a[3] = { Counter(1), Counter(2), Counter(3) }; }
    expect(6, 1, 2, 3, -3, -2, -1, 0, 0);

    // the elements left out are default-constructed
    { Counter a[2] = { Counter(1) }; }
    expect(4, 1, 9, -9, -1, 0, 0, 0, 0);

    // the length comes from the initializer
    { Counter a[] = { Counter(4), Counter(5) }; }
    expect(4, 4, 5, -5, -4, 0, 0, 0, 0);

    // brace elements call the constructor with those arguments
    { Counter a[2] = { { 6 }, { 7 } }; }
    expect(4, 6, 7, -7, -6, 0, 0, 0, 0);

    // both kinds in one list
    { Counter a[2] = { Counter(1), { 2 } }; }
    expect(4, 1, 2, -2, -1, 0, 0, 0, 0);

    // single objects: no temporary, no copy
    { Counter c = Counter(5); }
    expect(2, 5, -5, 0, 0, 0, 0, 0, 0);

    // an element that is an existing object is copy-constructed in place
    { Counter src(5); Counter a[2] = { src, Counter(6) }; }
    expect(6, 5, 105, 6, -6, -5, -5, 0, 0);

    { Plain src(3); Plain p[2] = { src, Plain(4) }; if (p[0].v != 3 || p[1].v != 4) e++; }

    // nested arrays, in row order and destroyed backwards
    { Counter a[2][2] = { { Counter(1), Counter(2) }, { Counter(3), { 4 } } }; if (a[1][1].id != 4) e++; }
    expect(8, 1, 2, 3, 4, -4, -3, -2, -1);

    // a nested array with elements left out, and one with no initializer
    { Counter a[2][2] = { { Counter(1) } }; }
    expect(8, 1, 9, 9, 9, -9, -9, -9, -1);
    { Counter a[2][2]; }
    expect(8, 9, 9, 9, 9, -9, -9, -9, -9);

    // a loop body: the array is built and destroyed on every pass
    for (int i = 0; i < 2; i++) { Counter a[2] = { Counter(1), Counter(2) }; if (a[1].id != 2) e++; }
    expect(8, 1, 2, -2, -1, 1, 2, -2, -1);

    // leaving through a return destroys the array too
    if (early(1) != 7) e++;
    if (early(0) != 8) e++;
    expect(8, 1, 2, -2, -1, 1, 2, -2, -1);

    // the first value of an element survives where it was built
    {
        Counter a[2] = { Counter(10), Counter(20) };
        if (a[0].id != 10 || a[1].id != 20) e++;
        if (g_live != 2) e++;
    }
    g_n = 0;

    test_errors = e;
    return 0;
}
