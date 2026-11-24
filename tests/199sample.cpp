// *****************************************************************************
//  tests/199sample.cpp -- `new` of a scalar: one word through the allocation hook.
//    new int / new int() / new int(5) / new float(2.5) / new int{7} / new Node*
//  and `delete` of them. `()` and `{}` give 0 / 0.0 / NULL, `(v)` and `{v}`
//  store v. Works with --alloc=arena and --alloc=gc.
//
//  test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

struct Node { int v; };
enum Shade { LIGHT, DARK };

int main(void)
{
    int e = 0;

    int *a = new int;
    *a = 3;
    int *b = new int();
    int *c = new int(5);
    int *d = new int{7};
    int *z = new int{};
    float *f = new float(2.5f);
    float *g = new float{};
    float *h = new float;
    *h = 0.5f;
    char *ch = new char('x');
    bool *bo = new bool(true);
    Shade *sh = new Shade(DARK);
    if (*a != 3 || *b != 0 || *c != 5 || *d != 7 || *z != 0) e++;
    if (*f != 2.5f || *g != 0.0f || *h != 0.5f) e++;
    if (*ch != 'x' || !*bo) e++;
    if (*sh != DARK) e++;

    Node n;
    n.v = 11;
    Node **np = new Node*;
    if (*np != 0) e++;
    *np = &n;
    if ((*np)->v != 11) e++;
    Node **np2 = new Node*(&n);
    if (*np2 != &n) e++;
    Node **np3 = new Node*{};
    if (*np3 != 0) e++;
    Node **np4 = new Node*(0);
    if (*np4 != 0) e++;

    // distinct, independent words
    *a = 1; *b = 2; *c = 3; *d = 4;
    if (*a != 1 || *b != 2 || *c != 3 || *d != 4) e++;

    delete a; delete b; delete c; delete d; delete z;
    delete f; delete g; delete h; delete ch; delete bo; delete sh;
    delete np; delete np2; delete np3; delete np4;

    // churn: the words come back (and under gc are collected)
    int sum = 0;
    for (int i = 0; i < 3000; i++) {
        int *t = new int(i);
        float *u = new float(1.0f);
        sum = sum + *t;
        delete t;
        delete u;
    }
    if (sum != 2999 * 3000 / 2) e++;

    test_errors = e;
    return 0;
}
