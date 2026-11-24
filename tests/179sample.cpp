// *****************************************************************************
//  tests/179sample.cpp -- `new T[n]` / `delete[]` run constructors and destructors
//
//  `new T[n]` default-constructs the n elements in order; for a class with a
//  destructor it keeps n in one header word in front of the elements, which
//  `delete[]` reads back to destroy them last first before freeing the block
//  (the pointer given to `delete[]` is not the one malloc returned). n may be
//  computed at run time, zero, or null for delete[]. A class without a
//  destructor has no header. A constructor logs its id, a destructor the
//  negated id; the default constructor gives 9, Wide's gives 5. test_errors
//  must end at 0. (Also built with --alloc=arena by `make test`.)
//
//  Not covered: `new T[n]{a, b}` (list-initialized array new) and delete[]
//  through a base pointer to a derived array, which is undefined in C++ too.
// *****************************************************************************

int test_errors = -1;
int g_log[64];
int g_n = 0;
int g_live = 0;

class D {
public:
    int id;
    D() { id = 9; g_log[g_n] = 9; g_n = g_n + 1; g_live = g_live + 1; }
    ~D() { g_log[g_n] = 0 - id; g_n = g_n + 1; g_live = g_live - 1; }
};

// elements numbered as they are built, to see the order
int g_serial = 0;
class Seq {
public:
    int id;
    Seq() { g_serial = g_serial + 1; id = g_serial; g_log[g_n] = id; g_n = g_n + 1; g_live = g_live + 1; }
    ~Seq() { g_log[g_n] = 0 - id; g_n = g_n + 1; g_live = g_live - 1; }
};

// a constructor and no destructor: constructed per element, no header
class Wide {
public:
    int a;
    int b;
    Wide() { a = 5; b = 6; g_live = g_live + 1; }
};

// a default argument makes it the default constructor
class Dflt {
public:
    int v;
    Dflt(int x = 42) { v = x; g_live = g_live + 1; }
    ~Dflt() { g_live = g_live - 1; }
};

// base and derived: each element runs the whole chain
class Base {
public:
    int b;
    Base() { b = 1; g_log[g_n] = 100; g_n = g_n + 1; g_live = g_live + 1; }
    ~Base() { g_log[g_n] = -100; g_n = g_n + 1; g_live = g_live - 1; }
};
class Derived : public Base {
public:
    int d;
    Derived() { d = 2; g_log[g_n] = 200; g_n = g_n + 1; g_live = g_live + 1; }
    ~Derived() { g_log[g_n] = -200; g_n = g_n + 1; g_live = g_live - 1; }
};

// virtual functions and no constructor: the vtable of every element is set
class Shape {
public:
    virtual int area() { return 1; }
};
class Square : public Shape {
public:
    int s;
    virtual int area() { return 4; }
};

int e = 0;

void check(const int *want, int n)
{
    if (g_n != n) e++;
    for (int i = 0; i < n && i < g_n; i++)
        if (g_log[i] != want[i]) e++;
    g_n = 0;
}

int count_for(int k) { return k * 2 - 1; }

int main(void)
{
    // construction in order, destruction last first, nothing alive afterwards
    {   D *p = new D[3];
        int w[] = { 9, 9, 9 };
        check(w, 3);
        if (g_live != 3) e++;
        delete[] p;
        int w2[] = { -9, -9, -9 };
        check(w2, 3);
        if (g_live != 0) e++;
    }

    // the order shows in the ids
    {   g_serial = 0;
        Seq *s = new Seq[4];
        if (s[0].id != 1 || s[3].id != 4) e++;
        delete[] s;
        int w[] = { 1, 2, 3, 4, -4, -3, -2, -1 };
        check(w, 8);
    }

    // n worked out at run time
    {   int n = count_for(3);                    // 5
        D *p = new D[n];
        if (g_live != 5) e++;
        p[4].id = 77;
        delete[] p;
        int w[] = { 9, 9, 9, 9, 9, -77, -9, -9, -9, -9 };
        check(w, 10);
    }

    // n == 0: a valid pointer, nothing built, nothing destroyed
    {   int zero = 0;
        D *p = new D[zero];
        if (p == NULL) e++;
        if (g_live != 0) e++;
        delete[] p;
        if (g_n != 0) e++;
        Wide *q = new Wide[zero];
        delete[] q;
    }

    // delete[] of a null pointer does nothing
    {   D *none = NULL;
        delete[] none;
        if (g_n != 0) e++;
    }

    // a class with a constructor and no destructor
    {   Wide *w = new Wide[3];
        if (w[0].a != 5 || w[2].b != 6 || g_live != 3) e++;
        delete[] w;
        g_live = 0;
    }

    // default arguments of the constructor
    {   Dflt *d = new Dflt[2];
        if (d[0].v != 42 || d[1].v != 42 || g_live != 2) e++;
        delete[] d;
        if (g_live != 0) e++;
    }

    // base and derived, element by element
    {   Derived *p = new Derived[2];
        int w[] = { 100, 200, 100, 200 };
        check(w, 4);
        if (p[1].b != 1 || p[1].d != 2) e++;
        delete[] p;
        int w2[] = { -200, -100, -200, -100 };
        check(w2, 4);
        if (g_live != 0) e++;
    }

    // vtables
    {   Square *q = new Square[2];
        Shape *s = &q[1];
        if (s->area() != 4) e++;
        delete[] q;
    }

    // plain data
    {   int *ints = new int[10];
        ints[9] = 3;
        if (ints[9] != 3) e++;
        delete[] ints;
    }

    // single objects are unaffected
    {   D *one = new D;
        delete one;
        int w[] = { 9, -9 };
        check(w, 2);
    }

    test_errors = e;
    return 0;
}
