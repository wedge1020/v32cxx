// *****************************************************************************
//  tests/176sample.cpp — array members of class type take initializers
//
//  `class Holder { Counter m[2] = { {1}, Counter(2) }; };` and
//  `Holder() : m{ {1}, {2} } {}` build the member array's elements in place,
//  element by element, in the constructor and before its body; the elements
//  left out are default-constructed (as for a local array). Holder's
//  destructor, written or implicit, destroys them last element first, after
//  its own body. A multi-dimensional member array is built and destroyed in
//  row order, and backwards. (This used to be an error: tests/174bad.cpp.)
//
//  Every form is run against a log: a constructor logs its id, a destructor
//  the negated id, the default constructor 9. test_errors must end at 0.
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

typedef Counter C;

// a typedef of the class names it as well
class ViaTypedef {
public:
    C m[2] = { { 11 }, C(12) };
};

// a default member initializer, and no constructor of its own: gets an implicit one (and destructor)
class Implicit {
public:
    Counter m[2] = { { 1 }, Counter(2) };
};

// a constructor that does not name `m`, and a destructor of its own that logs 50 after its body starts
class Plain {
public:
    int before;
    Counter m[3] = { { 4 }, { 5 } };   // [2] is default-constructed
    int after;
    Plain() { before = 0; after = 0; g_log[g_n] = 50; g_n = g_n + 1; }
    Plain(int x) { before = x; after = x; g_log[g_n] = 51; g_n = g_n + 1; }
    ~Plain() { g_log[g_n] = 60; g_n = g_n + 1; }
};

// the mem-initializer list names `m`: the default is not used, and the arguments come from the constructor
class Listed {
public:
    Counter m[2] = { { 100 }, { 200 } };   // never used by Listed(int)
    Listed(int a, int b) : m{ { a }, { b } } { g_log[g_n] = 70; g_n = g_n + 1; }
    Listed() : m{ { 7 } } { }              // partial: m[1] is default-constructed
    Listed(int a) : m{ Counter(a), a + 1 } { }   // an element given as a value
};

// two dimensions, written out and with elements left out; one more member after it
class Grid {
public:
    Counter g[2][2] = { { { 1 }, { 2 } }, { { 3 } } };   // g[1][1] default-constructed
    Counter last;
    Grid() { }
};

// out of line, with the list written outside the class
class Outside {
public:
    Counter m[2];
    Outside(int a);
};
Outside::Outside(int a) : m{ { a }, { a + 1 } } { }

// the member array is constructed between its neighbours in declaration order
class Order {
public:
    Counter first;
    Counter m[2] = { { 2 }, { 3 } };
    Counter last;
    Order() : first(1), last(4) { }
};

// the elements' arguments are ordinary expressions: another member, a parameter, an object (copied in place)
class Seeded {
public:
    int base;
    Counter m[2];
    Seeded(int x, const Counter &c) : base(x), m{ { base }, c } { }
};

int e = 0;

// compares the log with the first `n` values given (and that no object is left alive), then clears it
void expect(int n, int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
    int want[10];
    want[0] = a0; want[1] = a1; want[2] = a2; want[3] = a3; want[4] = a4;
    want[5] = a5; want[6] = a6; want[7] = a7; want[8] = a8; want[9] = a9;
    if (g_n != n) e++;
    for (int i = 0; i < n && i < 10; i++)
        if (g_log[i] != want[i]) e++;
    if (g_live != 0) e++;
    g_n = 0;
}

int main(void)
{
    // implicit constructor and destructor
    { Implicit h; if (h.m[0].id != 1 || h.m[1].id != 2) e++; if (g_live != 2) e++; }
    expect(4, 1, 2, -2, -1, 0, 0, 0, 0, 0, 0);

    { ViaTypedef v; if (v.m[0].id != 11 || v.m[1].id != 12) e++; }
    expect(4, 11, 12, -12, -11, 0, 0, 0, 0, 0, 0);

    // default initializer, a partial list, the body after the elements, the destructor body first
    { Plain p; if (p.m[0].id != 4 || p.m[1].id != 5 || p.m[2].id != 9) e++; }
    expect(8, 4, 5, 9, 50, 60, -9, -5, -4, 0, 0);
    { Plain p(3); if (p.before != 3 || p.m[1].id != 5) e++; }
    expect(8, 4, 5, 9, 51, 60, -9, -5, -4, 0, 0);

    // the list in the constructor wins over the member's default
    { Listed l(10, 20); if (l.m[0].id != 10 || l.m[1].id != 20) e++; }
    expect(5, 10, 20, 70, -20, -10, 0, 0, 0, 0, 0);
    { Listed l; if (l.m[0].id != 7 || l.m[1].id != 9) e++; }
    expect(4, 7, 9, -9, -7, 0, 0, 0, 0, 0, 0);
    { Listed l(30); if (l.m[0].id != 30 || l.m[1].id != 31) e++; }
    expect(4, 30, 31, -31, -30, 0, 0, 0, 0, 0, 0);

    // two dimensions: row order, destroyed backwards, then the member after it
    { Grid g; if (g.g[1][0].id != 3 || g.g[1][1].id != 9 || g.g[0][1].id != 2) e++; }
    expect(10, 1, 2, 3, 9, 9, -9, -9, -3, -2, -1);

    { Outside o(5); if (o.m[0].id != 5 || o.m[1].id != 6) e++; }
    expect(4, 5, 6, -6, -5, 0, 0, 0, 0, 0, 0);

    // declaration order between other members
    { Order o; if (o.m[1].id != 3) e++; }
    expect(8, 1, 2, 3, 4, -4, -3, -2, -1, 0, 0);

    // arguments: a member, an existing object (copy-constructed in place)
    { Counter src(8); Seeded s(5, src); if (s.m[0].id != 5 || s.m[1].id != 8) e++; }
    expect(6, 8, 5, 108, -8, -5, -8, 0, 0, 0, 0);

    // new / delete
    Plain *hp = new Plain(2);
    if (g_live != 3) e++;
    if (hp->m[0].id != 4) e++;
    delete hp;
    expect(8, 4, 5, 9, 51, 60, -9, -5, -4, 0, 0);
    Implicit *ip = new Implicit;
    delete ip;
    expect(4, 1, 2, -2, -1, 0, 0, 0, 0, 0, 0);

    // inside a loop, and an array of holders
    for (int i = 0; i < 2; i++) { Implicit h; }
    expect(8, 1, 2, -2, -1, 1, 2, -2, -1, 0, 0);
    { Implicit hs[2]; }
    expect(8, 1, 2, 1, 2, -2, -1, -2, -1, 0, 0);

    test_errors = e;
    return 0;
}
