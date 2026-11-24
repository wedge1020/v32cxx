// Struct, class and union definitions are written in dependency order: a type that holds another BY VALUE
// comes after it, whatever order the source declares them in (a union used to be printed before every
// struct, and the stock compiler refuses `union V { int a; B b; };` before `struct B`: "not fully defined",
// QUIRKS #35). A pointer member needs no order. Here the source order is already C++-valid; the output must
// follow the holds-by-value edges through unions, arrays, typedef names, inheritance and namespaces.
struct B { int x; int y; };
union V { int a; B b; float f; };
struct W { V v; B b2; int *p; };
struct Q;
struct P { Q *q; int k; };               // a pointer: no order needed
struct Q { P p; };
union Late;
namespace n { union N { W w; int i; }; }
struct Z { int a[2]; V v; };
union Late { int i; Z z; };
typedef Z ZA;
struct Holder { ZA arr[2]; };
struct H2 { n::N nn; };
class Base { public: V bv; };
class Derived : public Base { public: Z z; int tail; };
union Pick { Derived d; int raw; };
int test_errors = -1;
int main() {
    int e = 0;
    V v; v.a = 5;
    if (v.b.x != 5) e++;
    W w; w.b2.x = 3; w.v.b.y = 9;
    if (w.v.b.y != 9 || w.b2.x != 3) e++;
    Late l; l.z.a[1] = 4; l.z.v.a = 6;
    if (l.z.a[1] != 4 || l.z.v.a != 6) e++;
    Holder h; h.arr[1].v.b.x = 11;
    if (h.arr[1].v.b.x != 11) e++;
    H2 h2; h2.nn.w.b2.y = 12;
    if (h2.nn.w.b2.y != 12) e++;
    P p; Q q; p.q = &q; q.p.k = 8;
    if (p.q->p.k != 8) e++;
    Pick pk; pk.d.tail = 21; pk.d.bv.b.y = 22; pk.d.z.v.f = 1.5f;
    if (pk.d.tail != 21 || pk.d.bv.b.y != 22 || pk.d.z.v.f != 1.5f) e++;
    test_errors = e;
    return 0;
}
