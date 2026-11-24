// Value-initialization and zeroing of members whose type is a namespace-qualified class, struct, union or
// enum (`n::K k;`), seen through `using namespace`, a typedef, a nested namespace or an array: `new H`,
// `new H()`, `H h{}`, `new H[n]{}` / `new H[n]()` (the _rest helper), a short brace list, a union.
// The stack and the heap are dirtied first, so a zero that is not written shows up.
namespace n {
    struct K { int a; int b; };
    union N { int i; float f; };
    enum E { E0, E1 };
    typedef K KT;
    namespace in {
        struct J { int q; float r; };
        union W { K k; int w[3]; };
        enum F { F0, F1 };
    }
}
struct Holder {
    n::K k; n::N u; n::E e; n::in::J j; n::in::W w; n::in::F f;
    n::KT kt; n::K ka[2]; n::E ea[2]; int x;
};
using namespace n;
struct Holder2 { K k; N u; E e; in::J j; KT kt; in::W w; in::F f; K ka[2]; int *p; float fl; };
union Wrap { n::K k; int pad[4]; };
Holder2 g2;
Wrap gw;

int test_errors = -1;
int dirt_sink;
void dirty() {
    int junk[400];
    for (int i = 0; i < 400; i++) junk[i] = 0x5a5a5a5 + i;
    dirt_sink = junk[399];
}
// dirty the heap: take a block of garbage, give it back (default and gc builds reuse it; the arena rewinds)
void dirty_heap() {
#ifdef __V32_ARENA__
#endif
    int *g = new int[200];
    for (int i = 0; i < 200; i++) g[i] = 0x3b3b3b + i;
    delete[] g;
}

int bad_h(const Holder *h) {
    int e = 0;
    if (h->k.a != 0 || h->k.b != 0) e++;
    if (h->u.i != 0) e++;
    if ((int)h->e != 0) e++;
    if (h->j.q != 0 || h->j.r != 0.0f) e++;
    if (h->w.k.a != 0 || h->w.k.b != 0 || h->w.w[2] != 0) e++;
    if ((int)h->f != 0) e++;
    if (h->kt.a != 0 || h->kt.b != 0) e++;
    for (int i = 0; i < 2; i++) { if (h->ka[i].a != 0 || h->ka[i].b != 0) e++; if ((int)h->ea[i] != 0) e++; }
    if (h->x != 0) e++;
    return e;
}
int bad_h2(const Holder2 *h, int ptrs = 1) {
    int e = 0;
    if (h->k.a != 0 || h->k.b != 0) e++;
    if (h->u.i != 0) e++;
    if ((int)h->e != 0) e++;
    if (h->j.q != 0 || h->j.r != 0.0f) e++;
    if (h->kt.a != 0 || h->kt.b != 0) e++;
    if (h->w.k.a != 0 || h->w.k.b != 0 || h->w.w[2] != 0) e++;
    if ((int)h->f != 0) e++;
    for (int i = 0; i < 2; i++) if (h->ka[i].a != 0 || h->ka[i].b != 0) e++;
    if (ptrs && h->p != 0) e++;
    if (h->fl != 0.0f) e++;
    return e;
}

int local_braces() {
    dirty();
    Holder h{};
    return bad_h(&h);
}
int local_braces2() {
    dirty();
    Holder2 h{};
    return bad_h2(&h);
}
int local_eq() {
    dirty();
    Holder2 h = {};
    return bad_h2(&h);
}
int local_array() {
    dirty();
    Holder2 a[2] = {};
    return bad_h2(&a[0]) + bad_h2(&a[1]);
}
int local_short_list() {
    dirty();
    Holder2 h = { {1, 2} };
    int e = 0;
    if (h.k.a != 1 || h.k.b != 2) e++;
    h.k.a = 0; h.k.b = 0;
    return e + bad_h2(&h);
}
int local_union() {
    dirty();
    Wrap w = { {5, 6} };
    int e = 0;
    if (w.k.a != 5 || w.k.b != 6 || w.pad[2] != 0 || w.pad[3] != 0) e++;
    Wrap z = {};
    for (int i = 0; i < 4; i++) if (z.pad[i] != 0) e++;
    return e;
}
int local_qualified_union() {
    dirty();
    n::N u = {};
    n::in::W w = {};
    int e = 0;
    if (u.i != 0) e++;
    if (w.w[0] != 0 || w.w[1] != 0 || w.w[2] != 0) e++;
    return e;
}

int main() {
    int e = 0;
    // new, new(), new{}: the heap is dirty
    for (int round = 0; round < 3; round++) {
        dirty_heap();
        Holder *h = new Holder;
        e += bad_h(h);
        Holder2 *h2 = new Holder2;
        e += bad_h2(h2);
        if (round == 1) {
            h->k.a = 7; h->ka[1].b = 7; h2->j.q = 7; h2->w.k.a = 7; h2->e = E1;
        }
    }
    dirty_heap();
    { Holder *h = new Holder(); e += bad_h(h); Holder2 *h2 = new Holder2(); e += bad_h2(h2); }
    dirty_heap();
    { Holder *h = new Holder{}; e += bad_h(h); Holder2 *h2 = new Holder2{}; e += bad_h2(h2); }
    // arrays: `_rest`
    dirty_heap();
    {
        Holder *a = new Holder[3]{};
        for (int i = 0; i < 3; i++) e += bad_h(&a[i]);
        Holder2 *b = new Holder2[3]();
        for (int i = 0; i < 3; i++) e += bad_h2(&b[i]);
    }
    // scalar and aggregate members through pointers to the qualified types
    dirty_heap();
    {
        n::K *k = new n::K();
        if (k->a != 0 || k->b != 0) e++;
        n::K *ka = new n::K[3]{};
        for (int i = 0; i < 3; i++) if (ka[i].a != 0 || ka[i].b != 0) e++;
    }
    e += local_braces();
    e += local_braces2();
    e += local_eq();
    e += local_array();
    e += local_short_list();
    e += local_union();
    e += local_qualified_union();
    e += bad_h2(&g2, 0);
    for (int i = 0; i < 4; i++) { int *w = (int *)&gw; if (w[i] != 0) e++; }
    test_errors = e;
    return 0;
}
