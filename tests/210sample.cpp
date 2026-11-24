// `new` of a union (`new U`, `new U()`, `new U{}`, `new U{5}`, `new U[n]`, `new U[n]{}`, `new U[n]{ {1}, {2} }`,
// `delete`, `delete[]`, in a namespace too), of an enum (plain or `enum class`, in a namespace or not: `new E`,
// `new E(v)`, `new E[n]`, `new E[n]{a, b}`, `new E[n]()`), and `new T[n]{ T(), T(1, 2) }` / `{ {1, 2}, {} }` of an
// aggregate T (no constructor), mixed with a class that has constructors and with `new T*[n]{ new T(), nullptr }`.
// Every allocation goes through the hook, so the arena and gc builds run it too. The heap is dirtied first.
union U { int i; float f; };
union Big { int a; int words[8]; };                // more than four words: the zero is a loop
union PU { int *p; int q[2]; };                    // the first member is a pointer: NULL
namespace n {
    union V { int a; int b[3]; };
    enum G { G0, G1, G2 };
    enum class H : int { R, S, Z };
}
enum E { E0, E1, E2 };
enum class F { X, Y, Z };

struct T { int a; int b; T *p; E e; };
struct Row { int x; float f; int *q; };
class M {
public:
    int v;
    M() : v(7) {}
    M(int x) : v(x) {}
};
namespace n { struct S { int a; int b; }; union W { n::S s; int w[5]; }; }

int test_errors = -1;
void dirty_heap() {
    int *g = new int[300];
    for (int i = 0; i < 300; i++) g[i] = 0x3b3b3b + i;
    delete[] g;
}
void dirty_stack() {
    int junk[300];
    for (int i = 0; i < 300; i++) junk[i] = 0x5a5a5a + i;
    if (junk[299] == 0) test_errors = -2;
}

int main() {
    int e = 0;
    // ---- unions
    for (int round = 0; round < 2; round++) {
        dirty_heap();
        U *a = new U();
        if (a->i != 0 || a->f != 0.0f) e++;
        a->i = 77;
        dirty_heap();
        U *b = new U{};
        if (b->i != 0) e++;
        dirty_heap();
        U *c = new U{5};
        if (c->i != 5) e++;
        dirty_heap();
        U *d = new U[3]{};
        for (int i = 0; i < 3; i++) if (d[i].i != 0) e++;
        d[2].f = 1.5f;
        dirty_heap();
        U *f = new U[4]{ {1}, {2} };
        if (f[0].i != 1 || f[1].i != 2 || f[2].i != 0 || f[3].i != 0) e++;
        dirty_heap();
        U *g = new U[3]();
        for (int i = 0; i < 3; i++) if (g[i].i != 0) e++;
        U *h = new U;
        h->i = 4;
        U *pl = new U[2];
        pl[1].i = 9;
        if (pl[1].i != 9 || h->i != 4) e++;
        dirty_heap();
        Big *bg = new Big();
        for (int i = 0; i < 8; i++) if (bg->words[i] != 0) e++;
        dirty_heap();
        Big *bg2 = new Big{3};
        if (bg2->a != 3) e++;
        for (int i = 1; i < 8; i++) if (bg2->words[i] != 3 && i < 1) e++;
        dirty_heap();
        Big *ba = new Big[3]{};
        for (int k = 0; k < 3; k++) for (int i = 0; i < 8; i++) if (ba[k].words[i] != 0) e++;
        dirty_heap();
        PU *pu = new PU();
        if (pu->p != NULL) e++;
        PU *pa = new PU[2]{};
        if (pa[0].p != NULL || pa[1].p != NULL) e++;
        dirty_heap();
        n::V *v = new n::V;
        v->a = 1;
        n::V *v1 = new n::V();
        if (v1->a != 0 || v1->b[2] != 0) e++;
        n::V *v2 = new n::V{7};
        if (v2->a != 7) e++;
        n::V *v3 = new n::V[2]{};
        if (v3[1].b[2] != 0) e++;
        n::V *v4 = new n::V[3]{ {5} };
        if (v4[0].a != 5 || v4[1].a != 0 || v4[2].b[2] != 0) e++;
        n::W *w = new n::W{};
        if (w->s.a != 0 || w->w[4] != 0) e++;
        delete a; delete b; delete c; delete[] d; delete[] f; delete[] g; delete h; delete[] pl;
        delete bg; delete bg2; delete[] ba; delete pu; delete[] pa;
        delete v; delete v1; delete v2; delete[] v3; delete[] v4; delete w;
    }
    // ---- enums
    for (int round = 0; round < 2; round++) {
        dirty_heap();
        E *a = new E[3];
        a[0] = E1; a[2] = E2;
        if (a[0] != E1 || a[2] != E2) e++;
        E *b = new E[4]{ E1, E2 };
        if (b[0] != E1 || b[1] != E2 || b[2] != E0 || b[3] != E0) e++;
        dirty_heap();
        E *c = new E[3]();
        for (int i = 0; i < 3; i++) if (c[i] != E0) e++;
        E *d = new E(E2);
        if (*d != E2) e++;
        E *s = new E;
        *s = E1;
        if (*s != E1) e++;
        E *z = new E();
        if (*z != E0) e++;
        dirty_heap();
        F *f = new F[3]{ F::Z, F::Y };
        if (f[0] != F::Z || f[1] != F::Y || f[2] != F::X) e++;
        F *f2 = new F[2]{};
        if (f2[0] != F::X || f2[1] != F::X) e++;
        F *f3 = new F(F::Y);
        if (*f3 != F::Y) e++;
        dirty_heap();
        n::G *g = new n::G[3]{ n::G2, n::G1 };
        if (g[0] != n::G2 || g[1] != n::G1 || g[2] != n::G0) e++;
        n::G *g2 = new n::G[2];
        g2[1] = n::G1;
        n::H *h = new n::H[3]();
        for (int i = 0; i < 3; i++) if (h[i] != n::H::R) e++;
        n::H *h2 = new n::H[2]{ n::H::Z };
        if (h2[0] != n::H::Z || h2[1] != n::H::R) e++;
        n::H *h3 = new n::H(n::H::S);
        if (*h3 != n::H::S) e++;
        int cnt = 0;
        E *e5 = new E[5]{ E2, E2, E2, E2, E2 };
        for (int i = 0; i < 5; i++) if (e5[i] == E2) cnt++;
        if (cnt != 5) e++;
        delete[] a; delete[] b; delete[] c; delete d; delete s; delete z;
        delete[] f; delete[] f2; delete f3; delete[] g; delete[] g2; delete[] h; delete[] h2; delete h3; delete[] e5;
    }
    // ---- aggregates built from the list
    for (int round = 0; round < 2; round++) {
        dirty_heap();
        T *t = new T[4]{ T(), T(1, 2) };
        if (t[0].a != 0 || t[0].b != 0 || t[0].p != NULL || t[0].e != E0) e++;
        if (t[1].a != 1 || t[1].b != 2 || t[1].p != NULL) e++;
        if (t[2].a != 0 || t[2].p != NULL || t[3].b != 0) e++;
        dirty_heap();
        T *u = new T[3]{ {3, 4}, {} };
        if (u[0].a != 3 || u[0].b != 4 || u[0].p != NULL) e++;
        if (u[1].a != 0 || u[1].b != 0 || u[1].p != NULL || u[1].e != E0) e++;
        if (u[2].p != NULL) e++;
        dirty_heap();
        T t0;
        t0.a = 11; t0.b = 12; t0.p = &t0; t0.e = E2;
        T *v = new T[2]{ t0, T(t0) };
        if (v[0].a != 11 || v[1].b != 12 || v[1].p != &t0 || v[0].e != E2) e++;
        dirty_heap();
        T *w = new T[2]{ {1, 2, &t0, E1}, {5} };
        if (w[0].p != &t0 || w[0].e != E1 || w[1].a != 5 || w[1].b != 0 || w[1].p != NULL) e++;
        dirty_heap();
        Row *r = new Row[3]{ Row(), Row(8, 2.5f) };
        if (r[0].x != 0 || r[0].f != 0.0f || r[0].q != NULL) e++;
        if (r[1].x != 8 || r[1].f != 2.5f || r[1].q != NULL || r[2].q != NULL) e++;
        // the class with constructors, next to the aggregates
        M *m = new M[3]{ M(), M(5) };
        if (m[0].v != 7 || m[1].v != 5 || m[2].v != 7) e++;
        M *m2 = new M[2]{ {9}, {} };
        if (m2[0].v != 9 || m2[1].v != 7) e++;
        // a namespace's aggregate and union
        n::S *s = new n::S[3]{ {1, 2}, {} };
        if (s[0].a != 1 || s[0].b != 2 || s[1].a != 0 || s[2].b != 0) e++;
        n::W *ws = new n::W[2]{ { {1, 2} } };
        if (ws[0].s.a != 1 || ws[0].s.b != 2 || ws[0].w[4] != 0 || ws[1].w[3] != 0) e++;
        // an array of pointers: new T*[n]{ new T(), nullptr }
        dirty_heap();
        T **pp = new T*[3]{ new T(), nullptr };
        if (pp[0] == NULL || pp[0]->a != 0 || pp[0]->p != NULL || pp[1] != NULL || pp[2] != NULL) e++;
        T **pq = new T*[2]{ new T{7, 8}, new T() };
        if (pq[0]->a != 7 || pq[0]->b != 8 || pq[1]->b != 0) e++;
        delete pp[0]; delete pq[0]; delete pq[1];
        delete[] pp; delete[] pq;
        delete[] t; delete[] u; delete[] v; delete[] w; delete[] r; delete[] m; delete[] m2; delete[] s; delete[] ws;
    }
    test_errors = e;
    return 0;
}
