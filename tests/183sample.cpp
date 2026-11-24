// *****************************************************************************
//  tests/183sample.cpp -- a typedef as a base class
//
//  `typedef Base B; class D : public B {...};` -- the typedef names the base
//  (a chain of typedefs too), in the class head and in a constructor's
//  initializer list (`D() : B(3) {}`, in the class or out of line), and
//  virtual functions dispatch through it. test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

class Base {
public:
    int v;
    Base() { v = 1; }
    Base(int x) { v = x; }
    virtual int who() { return 10; }
    int twice() { return v * 2; }
};

typedef Base B;
typedef B B2;

class D : public B {
public:
    int d;
    D() : B(3) { d = 4; }
    D(int a, int b);
    virtual int who() { return 20; }
};

D::D(int a, int b) : B(a) { d = b; }

class E : public B2 {          // a chain of typedefs
public:
    E() : B2(7) {}
    virtual int who() { return 30; }
};

class F : public D {           // a class derived from a derived class
public:
    F() : D(5, 6) {}
};

int call(B *p) { return p->who(); }

int main(void)
{
    int e = 0;
    D d;
    if (d.v != 3 || d.d != 4) e++;
    if (d.twice() != 6) e++;
    D d2(8, 9);
    if (d2.v != 8 || d2.d != 9) e++;
    E x;
    if (x.v != 7 || x.who() != 30) e++;
    F f;
    if (f.v != 5 || f.d != 6 || f.who() != 20) e++;
    B plain;
    if (plain.v != 1 || plain.who() != 10) e++;

    // virtual calls through a pointer to the typedef'd base
    B *p = &d;
    if (p->who() != 20) e++;
    p = &x;
    if (p->who() != 30) e++;
    p = &f;
    if (call(p) != 20) e++;
    if (call(&plain) != 10) e++;

    // heap object through the typedef'd base
    B *h = new D(1, 2);
    if (h->who() != 20 || h->v != 1) e++;
    delete h;

    test_errors = e;
    return 0;
}
