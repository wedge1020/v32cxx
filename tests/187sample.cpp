// *****************************************************************************
//  tests/187sample.cpp -- `new T[n]{a, b, ...}`: list-initialized array new
//
//  The elements named in the braces are built from them (`{1, 2}` is a call of
//  the constructor with those arguments, `T(3)` and a plain `4` the same), the
//  rest of the n elements are default-constructed; `new int[n]{1, 2, 3}` zeroes
//  the rest. The count header of a class with a destructor is kept, so
//  `delete[]` runs every destructor, last first (also built with --alloc=arena
//  and --alloc=gc by `make test`). It works as a statement (`p = new ...;`,
//  `T *p = new ...;`) and inside any expression: an argument, a return value, a
//  member's value. Every constructor logs its id, a destructor the negated id.
//  With n smaller than the list the C++ program is undefined (it throws
//  std::bad_array_new_length): here the list's length is allocated. test_errors
//  must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_log[80];
int g_n = 0;
int g_live = 0;

class Item {
public:
    int id;
    int extra;
    Item() { id = 9; extra = 0; g_log[g_n] = 9; g_n = g_n + 1; g_live = g_live + 1; }
    Item(int i) { id = i; extra = 0; g_log[g_n] = i; g_n = g_n + 1; g_live = g_live + 1; }
    Item(int i, int x) { id = i; extra = x; g_log[g_n] = i; g_n = g_n + 1; g_live = g_live + 1; }
    ~Item() { g_log[g_n] = 0 - id; g_n = g_n + 1; g_live = g_live - 1; }
};

// no destructor: no count header
class Plain {
public:
    int a;
    Plain() { a = 5; }
    Plain(int x) { a = x; }
};

// virtual functions, a constructor with a default argument
class Shape {
public:
    int s;
    Shape(int v = 3) { s = v; }
    virtual int area() { return s; }
};

class Pair {
public:
    int a;
    int b;
};

// no default constructor: fine while the list covers every element
class Fixed {
public:
    int v;
    Fixed(int x) { v = x; }
};

int e = 0;

void check(const int *want, int n)
{
    if (g_n != n) e++;
    for (int i = 0; i < n && i < g_n; i++)
        if (g_log[i] != want[i]) e++;
    g_n = 0;
}

int count_for(int k) { return k + 1; }

int sum(const Item *p, int n)
{
    int s = 0;
    for (int i = 0; i < n; i++) s = s + p[i].id;
    return s;
}

Item *make(int n) { return new Item[n]{ Item(40), 41 }; }

int first_id(Item *p) { return p[0].id; }

class Holder {
public:
    Item *items;
    int *nums;
    Holder() : items(0), nums(0) {}
    void fill(int n) { items = new Item[n]{ 7, { 8, 80 } }; nums = new int[n]{ 5 }; }
    void drop() { delete[] items; delete[] nums; items = 0; nums = 0; }
};

int main(void)
{
    // a declaration: elements from the list, in order, then the rest default-constructed
    {   Item *p = new Item[5]{ 1, { 2, 20 }, Item(3) };
        int w[] = { 1, 2, 3, 9, 9 };
        check(w, 5);
        if (p[0].id != 1 || p[1].id != 2 || p[1].extra != 20 || p[2].id != 3 || p[3].id != 9 || p[4].id != 9) e++;
        if (g_live != 5) e++;
        delete[] p;
        int w2[] = { -9, -9, -3, -2, -1 };
        check(w2, 5);
        if (g_live != 0) e++;
    }

    // an assignment, n computed at run time
    {   Item *p = 0;
        int n = count_for(3);                         // 4
        p = new Item[n]{ 11, 12 };
        int w[] = { 11, 12, 9, 9 };
        check(w, 4);
        delete[] p;
        g_n = 0;
    }

    // the list as long as the array, and an empty list
    {   Item *p = new Item[2]{ 21, 22 };
        int w[] = { 21, 22 };
        check(w, 2);
        delete[] p;
        g_n = 0;
        Item *q = new Item[3]{};
        int w2[] = { 9, 9, 9 };
        check(w2, 3);
        delete[] q;
        g_n = 0;
    }

    // inside expressions: an argument, a return value, a member
    {   int s = sum(new Item[3]{ 1, 2, 3 }, 3);       // leaked on purpose: the point is the value
        if (s != 6) e++;
        g_live = 0;
        g_n = 0;
        Item *m = make(4);
        if (m[0].id != 40 || m[1].id != 41 || m[2].id != 9 || m[3].id != 9) e++;
        delete[] m;
        g_n = 0;
        if (first_id(new Item[2]{ 55 }) != 55) e++;
        g_n = 0;
        g_live = 0;
        Holder h;
        h.fill(3);
        if (h.items[0].id != 7 || h.items[1].extra != 80 || h.items[2].id != 9) e++;
        if (h.nums[0] != 5 || h.nums[1] != 0 || h.nums[2] != 0) e++;
        h.drop();
        if (g_live != 0) e++;
        g_n = 0;
    }

    // scalars: the rest is zero
    {   int n = count_for(5);                         // 6
        int *a = new int[n]{ 1, 2, 3 };
        if (a[0] != 1 || a[1] != 2 || a[2] != 3 || a[3] != 0 || a[4] != 0 || a[5] != 0) e++;
        a[5] = 7;
        delete[] a;
        float *f = new float[4]{ 1.5, 2.5 };
        if (f[0] != 1.5 || f[1] != 2.5 || f[2] != 0 || f[3] != 0) e++;
        delete[] f;
        int *b = new int[3]{ 7, count_for(1), 9 };    // values may be expressions
        if (b[1] != 2) e++;
        delete[] b;
    }

    // a class without a destructor (no header), with virtual functions and default arguments
    {   Plain *p = new Plain[3]{ 8 };
        if (p[0].a != 8 || p[1].a != 5 || p[2].a != 5) e++;
        delete[] p;
        Shape *s = new Shape[3]{ 4 };
        if (s[0].area() != 4 || s[1].area() != 3 || s[2].area() != 3) e++;
        Shape *t = &s[1];
        if (t->area() != 3) e++;
        delete[] s;
    }

    // a class with no constructor: elements are copied from the list, the rest is zero
    {   Pair q;
        q.a = 3;
        q.b = 4;
        Pair *p = new Pair[3]{ q };
        if (p[0].a != 3 || p[0].b != 4 || p[1].a != 0 || p[2].b != 0) e++;
        delete[] p;
    }

    // every element named: no default constructor needed
    {   Fixed *f = new Fixed[2]{ 6, 7 };
        if (f[0].v != 6 || f[1].v != 7) e++;
        delete[] f;
    }

    // n smaller than the list (undefined in C++): the list's length is allocated
    {   int n = 1;
        int *a = new int[n]{ 1, 2, 3 };
        if (a[0] != 1 || a[2] != 3) e++;
        delete[] a;
    }

    test_errors = e;
    return 0;
}
