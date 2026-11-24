// *****************************************************************************
//  tests/198sample.cpp -- list-initialization in three more places (see 175):
//    (a) in-class default member initializers with braces: `P m{}; int w{5}; int a[3]{1, 2};`
//    (b) a braced mem-initializer on a scalar or an aggregate member: `: w{5}`, `: p{}`
//    (c) `new P{}`, `new P{1, 2}`, `new Counter{3}`
//  A class with a constructor is built by it, an aggregate takes its members in
//  order and zero for the rest (a pointer member is NULL), a scalar its value.
//
//  test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_live = 0;

class Counter {
public:
    int id;
    Counter() { id = 9; g_live = g_live + 1; }
    Counter(int i) { id = i * 10; g_live = g_live + 1; }
    Counter(int i, int j) { id = i + j; g_live = g_live + 1; }
    ~Counter() { g_live = g_live - 1; }
};

struct P { int x; int y; };
struct Q { int a; P in; int *ptr; float f; };

// (a)
class H {
public:
    P m{};
    P n{1, 2};
    int w{5};
    Counter c{3};
    int *p{};
    Q q{7};
    float fl{1.5f};
    int arr[3]{1, 2};
    int *parr[2]{};
    float farr[2] = {0.5f};
};

// (b)
class K {
public:
    int w;
    int v;
    P p;
    P r;
    int *ptr;
    Counter c;
    K() : w{}, v{6}, p{}, r{3}, ptr{}, c{4} {}
    K(int a) : w{a}, v(a + 1), p{a, a + 1}, r{}, ptr{}, c{} {}
};

int main(void)
{
    int e = 0;
    int base = g_live;

    {
        H h;
        if (h.m.x != 0 || h.m.y != 0) e++;
        if (h.n.x != 1 || h.n.y != 2) e++;
        if (h.w != 5) e++;
        if (h.c.id != 30) e++;
        if (h.p != 0) e++;
        if (h.q.a != 7 || h.q.in.x != 0 || h.q.in.y != 0 || h.q.ptr != 0 || h.q.f != 0.0f) e++;
        if (h.fl != 1.5f) e++;
        if (h.arr[0] != 1 || h.arr[1] != 2 || h.arr[2] != 0) e++;
        if (h.parr[0] != 0 || h.parr[1] != 0) e++;
        if (h.farr[0] != 0.5f || h.farr[1] != 0.0f) e++;
        int loc[3]{4, 5, 6};
        if (loc[0] != 4 || loc[2] != 6) e++;
        if (g_live != base + 1) e++;
    }
    if (g_live != base) e++;

    {
        K k;
        if (k.w != 0 || k.v != 6) e++;
        if (k.p.x != 0 || k.p.y != 0) e++;
        if (k.r.x != 3 || k.r.y != 0) e++;
        if (k.ptr != 0) e++;
        if (k.c.id != 40) e++;
        K k2(8);
        if (k2.w != 8 || k2.v != 9) e++;
        if (k2.p.x != 8 || k2.p.y != 9) e++;
        if (k2.r.x != 0 || k2.r.y != 0 || k2.ptr != 0) e++;
        if (k2.c.id != 9) e++;
    }
    if (g_live != base) e++;

    {
        P *a = new P{};
        P *b = new P{1, 2};
        P *c0 = new P{5};
        Q *q = new Q{1, {2, 3}, 0, 2.5f};
        Q *q2 = new Q{};
        Q *q3 = new Q{9};
        Counter *c = new Counter{3};
        Counter *d = new Counter{};
        Counter *f = new Counter{1, 2};
        if (a->x != 0 || a->y != 0) e++;
        if (b->x != 1 || b->y != 2) e++;
        if (c0->x != 5 || c0->y != 0) e++;
        if (q->a != 1 || q->in.x != 2 || q->in.y != 3 || q->ptr != 0 || q->f != 2.5f) e++;
        if (q2->a != 0 || q2->in.y != 0 || q2->ptr != 0 || q2->f != 0.0f) e++;
        if (q3->a != 9 || q3->in.x != 0 || q3->ptr != 0) e++;
        if (c->id != 30 || d->id != 9 || f->id != 3) e++;
        if (g_live != base + 3) e++;
        delete a; delete b; delete c0; delete q; delete q2; delete q3;
        delete c; delete d; delete f;
        if (g_live != base) e++;
    }

    test_errors = e;
    return 0;
}
