// *****************************************************************************
//  tests/197sample.cpp -- brace-initialized unions (QUIRKS #33 lowered).
//
//  The stock compiler rejects `U u = {1};` and `{}`. v32c++ declares the union
//  and assigns the first member (C: a brace list initializes the first member),
//  zeroing the rest of the union; file-scope ones are set at the start of main.
//
//  test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

union U { int a; float f; };
union W { int a; int b[6]; };
union FU { float f; int a; };
union PU { int *p; int x[3]; };
struct S { int k; U u; };
struct T { int *q; W w; S s[2]; float f; };

U g1 = {7};
U g2{};
U g3 = {};
W gw = {3};
PU gp{};
S gs = {1, {2}};
S gs2{};
U ga[3] = {{1}, {2}};
T gt = {0, {5}, {{1, {2}}}, 1.5f};
U gu[] = {{4}, {5}, {6}};

namespace ns { U nu = {9}; W nw{}; }

int dirty(int n)
{
    int junk[40];
    for (int i = 0; i < 40; i++) junk[i] = n + i;
    return junk[39];
}

int main(void)
{
    int e = 0;

    if (g1.a != 7) e++;
    if (g2.a != 0 || g3.a != 0) e++;
    if (gw.a != 3 || gw.b[1] != 0 || gw.b[5] != 0) e++;
    if (gp.p != 0 || gp.x[1] != 0 || gp.x[2] != 0) e++;
    if (gs.k != 1 || gs.u.a != 2 || gs2.k != 0 || gs2.u.a != 0) e++;
    if (ga[0].a != 1 || ga[1].a != 2 || ga[2].a != 0) e++;
    if (gt.q != 0 || gt.w.a != 5 || gt.w.b[4] != 0 || gt.s[0].k != 1 || gt.s[0].u.a != 2 ||
        gt.s[1].k != 0 || gt.s[1].u.a != 0 || gt.f != 1.5f) e++;
    if (ns::nu.a != 9 || ns::nw.a != 0 || ns::nw.b[5] != 0) e++;
    if (gu[0].a != 4 || gu[1].a != 5 || gu[2].a != 6 || sizeof(gu) != 3 * sizeof(U)) e++;

    dirty(100);
    {
        U u = {1};
        FU f = {2.5f};
        U z{};
        U z2 = {};
        W w = {8};
        W wz{};
        PU pu{};
        S s = {3, {4}};
        S s0{};
        U arr[2] = {{1}, {2}};
        T t = {0, {5}, {{1, {2}}, {3, {4}}}, 1.5f};
        T t0{};
        U big[10] = {{1}};
        if (u.a != 1) e++;
        if (f.f != 2.5f) e++;
        if (z.a != 0 || z2.a != 0) e++;
        if (w.a != 8) e++;
        for (int i = 1; i < 6; i++) if (w.b[i] != 0) e++;
        for (int i = 0; i < 6; i++) if (wz.b[i] != 0) e++;
        if (pu.p != 0 || pu.x[1] != 0 || pu.x[2] != 0) e++;
        if (s.k != 3 || s.u.a != 4) e++;
        if (s0.k != 0 || s0.u.a != 0) e++;
        if (arr[0].a != 1 || arr[1].a != 2) e++;
        if (t.q != 0 || t.w.a != 5 || t.w.b[5] != 0 || t.s[0].k != 1 || t.s[1].u.a != 4 || t.f != 1.5f) e++;
        if (t0.q != 0 || t0.w.a != 0 || t0.w.b[3] != 0 || t0.s[1].k != 0 || t0.s[1].u.a != 0 || t0.f != 0.0f) e++;
        if (big[0].a != 1) e++;
        for (int i = 1; i < 10; i++) if (big[i].a != 0) e++;
    }

    dirty(200);
    {
        S *p = new S();
        if (p->k != 0 || p->u.a != 0) e++;
        S *pa = new S[3]{};
        for (int i = 0; i < 3; i++) if (pa[i].k != 0 || pa[i].u.a != 0) e++;
        T *tp = new T();
        if (tp->q != 0 || tp->w.b[5] != 0 || tp->s[1].u.a != 0) e++;
        T *ta = new T[2]{};
        for (int i = 0; i < 2; i++) if (ta[i].q != 0 || ta[i].w.b[2] != 0 || ta[i].f != 0.0f) e++;
        delete p; delete[] pa; delete tp; delete[] ta;
    }

    test_errors = e;
    return 0;
}
