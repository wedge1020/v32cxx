// `new T[n]()`: a value-initialized array new, the same as `new T[n]{}`: every element is
// value-initialized (a class gets its constructor, a number or a pointer is zero).
struct P { int x; int y; P() { x = 7; y = 8; } };
struct Q { int a; float f; int *p; };
struct D { int v; D() { v = 1; } ~D() { v = 0; } };
int test_errors = -1;
int main() {
    int e = 0;
    int n = 3;
    int *a = new int[n]();
    for (int i = 0; i < n; i++) if (a[i] != 0) e++;
    float *fl = new float[n]();
    for (int i = 0; i < n; i++) if (fl[i] != 0.0f) e++;
    P *p = new P[n]();
    for (int i = 0; i < n; i++) if (p[i].x != 7 || p[i].y != 8) e++;
    Q *q = new Q[n]();
    for (int i = 0; i < n; i++) if (q[i].a != 0 || q[i].f != 0.0f || q[i].p != 0) e++;
    int **pp = new int *[n]();
    for (int i = 0; i < n; i++) if (pp[i] != 0) e++;
    D *d = new D[2]();
    if (d[0].v != 1 || d[1].v != 1) e++;
    // the memory is reused after a delete[]: the next array is still zero
    a[0] = 9; a[1] = 9; a[2] = 9;
    delete[] a;
    int *b = new int[n]();
    for (int i = 0; i < n; i++) if (b[i] != 0) e++;
    delete[] b; delete[] fl; delete[] p; delete[] q; delete[] pp; delete[] d;
    test_errors = e;
    return 0;
}
