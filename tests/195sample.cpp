// *****************************************************************************
//  tests/195sample.cpp -- the zero of a short initializer, on the real machine
//
//  The Vircon32 compiler leaves a LOCAL's tail unwritten when a braced list is
//  short (`P z{};`, `int a[4] = {0};`, `P p{1};`), and a null pointer is NULL
//  (-1), not 0. v32c++ writes every list out in full (nested structs and arrays
//  too, a pointer slot as NULL), zeroes what `new P()` / `new P[n]{}` / `: m{}`
//  make, and prints a literal 0 as NULL in a pointer initializer at file scope
//  and through a `T*&`. dirty() fills the stack below with non-zero words first,
//  so a local that is not really zeroed fails. Self-checking (test_errors).
// *****************************************************************************
int test_errors = -1;

struct P { int x; int y; };
struct Q { P a; int b[3]; int c; };
struct R { int n; int *p; float f; };
class K2 { public: P m; R r; int w; K2() : m{}, r{} { w = 1; } };

R gr = {1};
R gra[2] = {{1}};
int garr[3] = {1};
R *g_rp = 0;
R *g_rtab[2] = {0, 0};

int dirty(int n)
{
    int pad[64];
    int s = 0;
    for (int i = 0; i < 64; i++) { pad[i] = n + 100; s += pad[i]; }
    return s;
}

int t1() { P z{}; return z.x == 0 && z.y == 0; }
int t2() { P z = {}; return z.x == 0 && z.y == 0; }
int t3() { P z{1}; return z.x == 1 && z.y == 0; }
int t4() { int a[4] = {0}; return a[0] == 0 && a[3] == 0; }
int t5() { int a[4] = {}; return a[1] == 0 && a[3] == 0; }
int t6() { Q q{}; return q.a.y == 0 && q.b[2] == 0 && q.c == 0; }
int t7() { R r{}; return r.n == 0 && r.p == 0 && r.f == 0.0f; }
int t8() { R ra[2] = {}; return ra[1].p == 0 && ra[1].n == 0 && ra[0].f == 0.0f; }
int t9() { float f{}; int *ip{}; int s{}; return f == 0.0f && ip == 0 && s == 0; }
int t10() { P *p = new P(); int r = p->x == 0 && p->y == 0; delete p; return r; }
int t11() { P *p = new P[3]{}; int r = p[2].x == 0 && p[2].y == 0 && p[0].y == 0; delete[] p; return r; }
int t12() { int *p = new int[5]{}; int r = p[4] == 0 && p[0] == 0; delete[] p; return r; }
int t13() { R *p = new R(); int r = p->p == 0 && p->n == 0; delete p; return r; }
int t14() { return gr.n == 1 && gr.p == 0 && gr.f == 0.0f && gra[1].p == 0 && gra[1].n == 0 && garr[2] == 0 && garr[0] == 1; }
int t15() { K2 k; return k.m.x == 0 && k.m.y == 0 && k.r.p == 0 && k.r.n == 0 && k.w == 1; }
int t16() { R *a[2] = {0, 0}; return a[0] == 0 && a[1] == 0 && g_rp == 0 && g_rtab[1] == 0; }
void clear(R*& p) { p = 0; }
int t17() { R r; R *p = &r; clear(p); return p == 0 && !p; }

int main()
{
    int e = 0;
    dirty(1); if (!t1()) e |= 1;
    dirty(2); if (!t2()) e |= 2;
    dirty(3); if (!t3()) e |= 4;
    dirty(4); if (!t4()) e |= 8;
    dirty(5); if (!t5()) e |= 16;
    dirty(6); if (!t6()) e |= 32;
    dirty(7); if (!t7()) e |= 64;
    dirty(8); if (!t8()) e |= 128;
    dirty(9); if (!t9()) e |= 256;
    dirty(1); if (!t10()) e |= 512;
    dirty(1); if (!t11()) e |= 1024;
    dirty(1); if (!t12()) e |= 2048;
    dirty(1); if (!t13()) e |= 4096;
    dirty(1); if (!t14()) e |= 8192;
    dirty(1); if (!t15()) e |= 16384;
    dirty(1); if (!t16()) e |= 32768;
    dirty(1); if (!t17()) e |= 65536;
    test_errors = e;
    return 0;
}
