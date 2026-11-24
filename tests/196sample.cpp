// *****************************************************************************
//  tests/196sample.cpp -- value-initialization gives NULL (-1 on Vircon32) to
//  pointer members: `new P[n]{}`, `new P()`, `new P{}`-free forms, nested
//  structs and arrays of pointers, `new T*[n]{}`, file-scope lists.
//
//  test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

struct P { int x; P *next; float f; };
struct Q { P in; P *arr[2]; int k; };
struct R { int a; Q q[2]; };
enum Mode { M_A, M_B };
struct W { Mode m; P *p; Mode ms[2]; };      // the stock compiler takes no int for an enumeration: written (Mode)0

P g_p = {1};
Q g_q = {{1, 0, 2.0f}};

int main(void)
{
    int e = 0;

    P *a = new P[3]{};
    for (int i = 0; i < 3; i++) {
        if (a[i].x != 0) e++;
        if (a[i].next != 0) e++;
        if (a[i].f != 0.0f) e++;
    }
    P *b = new P[2]{}; b[0].x = 5;
    if (b[0].x != 5 || b[0].next != 0) e++;
    if (b[1].x != 0 || b[1].next != 0 || b[1].f != 0.0f) e++;

    P *c = new P();
    if (c->x != 0 || c->next != 0 || c->f != 0.0f) e++;

    Q *q = new Q();
    if (q->in.next != 0 || q->arr[0] != 0 || q->arr[1] != 0 || q->k != 0) e++;
    Q *qa = new Q[2]{};
    for (int i = 0; i < 2; i++)
        if (qa[i].in.next != 0 || qa[i].arr[0] != 0 || qa[i].arr[1] != 0 || qa[i].k != 0) e++;

    R *r = new R();
    for (int i = 0; i < 2; i++)
        if (r->q[i].in.next != 0 || r->q[i].arr[1] != 0) e++;
    R *ra = new R[2]{};
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            if (ra[i].q[j].in.next != 0 || ra[i].q[j].arr[0] != 0 || ra[i].a != 0) e++;

    P **pp = new P*[4]{};
    for (int i = 0; i < 4; i++) if (pp[i] != 0) e++;
    P **pq = new P*[4]{a, b};
    if (pq[0] != a || pq[1] != b || pq[2] != 0 || pq[3] != 0) e++;

    if (g_p.next != 0 || g_p.x != 1) e++;
    if (g_q.in.next != 0 || g_q.arr[0] != 0 || g_q.arr[1] != 0 || g_q.k != 0 || g_q.in.f != 2.0f) e++;

    W *w = new W();
    if (w->m != M_A || w->p != 0 || w->ms[1] != M_A) e++;
    W *wa = new W[2]{};
    if (wa[1].m != M_A || wa[1].p != 0 || wa[0].ms[0] != M_A) e++;
    W wl{};
    W wl2 = {M_B};
    if (wl.m != M_A || wl.p != 0 || wl.ms[1] != M_A) e++;
    if (wl2.m != M_B || wl2.p != 0 || wl2.ms[0] != M_A) e++;
    delete w; delete[] wa;

    delete[] a; delete[] b; delete c; delete q; delete[] qa; delete r; delete[] ra;
    delete[] pp; delete[] pq;

    test_errors = e;
    return 0;
}
