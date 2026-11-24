// Operator overloading, the rest of C++'s set: % & | ^ ~ << >> and their compound forms, unary + - ~ *, ->, && ||
// and `,`, as members and as free functions (friends, namespace ones with out-of-line definitions), with mixed
// operand types, const references, by-value operands and returns by value, reference and pointer. Built-in
// operators must stay built-in, and precedence must be the one C++ gives (the sample compares each expression
// on a wrapper class with the same expression on ints). Unary `&` is not overloadable (see 213bad).
int test_errors = -1;
int e = 0;

// ---- a bitset-style class: every bitwise operator, members -------------------------------------------------------
class Bits {
public:
    int w;
    Bits() : w(0) {}
    Bits(int x) : w(x) {}
    Bits operator&(const Bits &o) const { return Bits(w & o.w); }
    Bits operator|(const Bits &o) const { return Bits(w | o.w); }
    Bits operator^(const Bits &o) const { return Bits(w ^ o.w); }
    Bits operator~() const { return Bits(~w); }
    Bits operator<<(int n) const { return Bits(w << n); }
    Bits operator>>(int n) const { return Bits(w >> n); }
    Bits operator%(int m) const { return Bits(w % m); }
    Bits &operator&=(const Bits &o) { w &= o.w; return *this; }
    Bits &operator|=(const Bits &o) { w |= o.w; return *this; }
    Bits &operator^=(const Bits &o) { w ^= o.w; return *this; }
    Bits &operator<<=(int n) { w <<= n; return *this; }
    Bits &operator>>=(int n) { w >>= n; return *this; }
    Bits &operator%=(int m) { w %= m; return *this; }
    bool operator==(const Bits &o) const { return w == o.w; }
    bool operator!() const { return w == 0; }
    Bits operator+() const { return Bits(w + 100); }       // distinguishable from the identity
    Bits operator-() const { return Bits(-w); }
};

static int bits_test() {
    int r = 0;
    Bits a(12), b(10);
    if ((a & b).w != 8) r++;
    if ((a | b).w != 14) r++;
    if ((a ^ b).w != 6) r++;
    if ((~a).w != -13) r++;
    if ((a << 2).w != 48) r++;
    if ((a >> 2).w != 3) r++;
    if ((a % 5).w != 2) r++;
    if ((+a).w != 112) r++;
    if ((-a).w != -12) r++;
    Bits c(12);
    c &= b;
    if (c.w != 8) r++;
    c |= Bits(3);
    if (c.w != 11) r++;
    c ^= Bits(1);
    if (c.w != 10) r++;
    c <<= 3;
    if (c.w != 80) r++;
    c >>= 4;
    if (c.w != 5) r++;
    c %= 3;
    if (c.w != 2) r++;
    Bits d(1);
    (d <<= 1) <<= 2;                     // the reference returned is the object itself
    if (d.w != 8) r++;
    Bits &rd = (d >>= 1);
    rd <<= 3;
    if (d.w != 32) r++;
    if (!a) r++;
    if (!Bits(0) == false) r++;
    Bits *pa = &a;                       // `&` on an object is the address even though Bits has a binary operator&
    if (pa->w != 12 || (&b)->w != 10) r++;
    // built-in operators on ints are untouched
    int x = 12, y = 10;
    if ((x & y) != 8 || (x | y) != 14 || (x ^ y) != 6 || (~x) != -13 || (x << 2) != 48 || (x >> 2) != 3 || x % 5 != 2) r++;
    x &= y; x |= 3; x ^= 1; x <<= 3; x >>= 4; x %= 3;
    if (x != 2) r++;
    if (+x != 2 || -x != -2) r++;
    return r;
}

class Fixed;
Fixed fx(int r);
// ---- a fixed-point class: members, mixed operands (v * 2, 2 * v), free functions and friends --------------------
class Fixed {
public:
    int raw;                              // 8 fractional bits
    Fixed() : raw(0) {}
    Fixed(int whole) : raw(whole * 256) {}
    Fixed operator*(int k) const { return fx(raw * k); }
    Fixed operator*(const Fixed &o) const { return fx((raw * o.raw) / 256); }
    Fixed operator%(const Fixed &o) const { return fx(raw % o.raw); }
    Fixed operator<<(int n) const { return fx(raw << n); }
    Fixed operator-() const { return fx(-raw); }
    friend Fixed operator&(const Fixed &a, int mask);
    friend Fixed operator|(int mask, const Fixed &a);
};
Fixed fx(int r) { Fixed f; f.raw = r; return f; }
Fixed operator&(const Fixed &a, int mask) { return fx(a.raw & mask); }
Fixed operator|(int mask, const Fixed &a) { return fx(a.raw | mask); }
Fixed operator*(int k, const Fixed &f) { return fx(f.raw * k + 1); }     // +1: tells it from the member
Fixed operator^(Fixed a, Fixed b) { return fx(a.raw ^ b.raw); }         // by value operands
Fixed operator>>(Fixed a, int n) { return fx(a.raw / (1 << n)); }
Fixed &operator+=(Fixed &a, int k) { a.raw += k; return a; }

static int fixed_test() {
    int r = 0;
    Fixed v(3), h(2);
    if ((v * 2).raw != 1536) r++;                // member operator*(int)
    if ((2 * v).raw != 1537) r++;                // free operator*(int, const Fixed &)
    if ((v * h).raw != 1536) r++;                // member operator*(const Fixed &)
    if ((v % fx(500)).raw != 268) r++;
    if ((v << 1).raw != 1536) r++;
    if ((-v).raw != -768) r++;
    if ((v & 255).raw != 0) r++;                 // friend, Fixed first
    if ((v & 1023).raw != 768) r++;
    if ((5 | v).raw != 773) r++;                 // friend, int first
    if ((v ^ h).raw != (768 ^ 512)) r++;
    if ((v >> 2).raw != 192) r++;
    v += 4;
    if (v.raw != 772) r++;
    // precedence is the C++ one: `*` binds tighter than `+`-less `^`, `<<` tighter than `&`
    Fixed p = 2 * v ^ h;                         // (2 * v) ^ h
    if (p.raw != ((772 * 2 + 1) ^ 512)) r++;
    return r;
}

// ---- free operators in a namespace, defined out of line -----------------------------------------------------------
namespace ns {
    struct M { int v; };
    M operator%(M a, M b);
    M operator|(const M &a, int k);
    M &operator<<=(M &a, int n);
    int operator&&(M a, M b);
    namespace in {
        struct N { int q; };
        N operator^(N a, N b);
    }
}
ns::M ns::operator%(ns::M a, ns::M b) { ns::M r; r.v = a.v % b.v; return r; }
ns::M ns::operator|(const ns::M &a, int k) { ns::M r; r.v = a.v | k; return r; }
ns::M &ns::operator<<=(ns::M &a, int n) { a.v <<= n; return a; }
int ns::operator&&(ns::M a, ns::M b) { return a.v * 10 + b.v; }
ns::in::N ns::in::operator^(ns::in::N a, ns::in::N b) { ns::in::N r; r.q = a.q ^ b.q; return r; }

static int ns_test() {
    int r = 0;
    ns::M a, b;
    a.v = 17; b.v = 5;
    if ((a % b).v != 2) r++;
    if ((a | 6).v != 23) r++;
    a <<= 2;
    if (a.v != 68) r++;
    if ((a && b) != 685) r++;
    ns::in::N p, q;
    p.q = 6; q.q = 3;
    if ((p ^ q).q != 5) r++;
    return r;
}

// ---- a smart pointer: operator* and operator->, one level and two levels -----------------------------------------
struct Node {
    int val;
    int next;
    int twice() { return val * 2; }
};
class Ptr {
public:
    Node *p;
    Ptr() : p(0) {}
    Ptr(Node *q) : p(q) {}
    Node &operator*() const { return *p; }
    Node *operator->() const { return p; }
};
class Ptr2 {
public:
    Ptr inner;
    Ptr2(Node *q) : inner(q) {}
    Ptr operator->() const { return inner; }        // a class again: -> is applied once more
};
class Ptr3 {                                        // three levels, through a reference return
public:
    Ptr2 mid;
    Ptr3(Node *q) : mid(q) {}
    Ptr2 &operator->() { return mid; }
};
Node &operator*(const Ptr2 &s) { return *s.inner.p; }     // a free unary operator*

static int const_read(const Ptr &cp) { return cp->val + (*cp).val - 11; }

static int ptr_test() {
    int r = 0;
    Node n;
    n.val = 7; n.next = 1;
    Ptr p(&n);
    if ((*p).val != 7) r++;
    if (p->val != 7) r++;
    if (p->twice() != 14) r++;
    p->val = 9;
    if (n.val != 9) r++;
    (*p).next = 4;
    if (n.next != 4) r++;
    *p = n;                                           // assigns through the returned reference
    Node m;
    m.val = 100; m.next = 200;
    *p = m;
    if (n.val != 100 || n.next != 200) r++;
    Ptr2 pp(&n);
    if (pp->val != 100) r++;                          // (pp.operator->())->val
    pp->val = 11;
    if (n.val != 11) r++;
    if (pp->twice() != 22) r++;
    if ((*pp).next != 200) r++;                       // free operator*
    Ptr3 p3(&n);
    if (p3->val != 11) r++;
    p3->next = 5;
    if (n.next != 5) r++;
    if (const_read(p) != 11) r++;                     // a const object: const operator-> and operator*
    // plain pointers keep the built-in * and ->
    Node *raw = &n;
    if ((*raw).val != 11 || raw->val != 11 || raw->twice() != 22) r++;
    Node **pr = &raw;
    if ((*pr)->val != 11) r++;
    return r;
}

// ---- stream-style <<, chained, into a buffer ---------------------------------------------------------------------
int seq = 0;
int nextval() { seq++; return seq; }
struct Pair { int a; int b; };
class Out {
public:
    char buf[96];
    int n;
    Out() : n(0) { buf[0] = 0; }
    Out &put(char c) { buf[n] = c; n++; buf[n] = 0; return *this; }
    Out &operator<<(char c) { return put(c); }
    Out &operator<<(const char *s) { while (*s) { put(*s); s++; } return *this; }
    Out &operator<<(int v) {
        if (v < 0) { put('-'); v = -v; }
        if (v >= 10) *this << (v / 10);
        return put('0' + (v % 10));
    }
    Out &operator<<(bool b) { return *this << (b ? "yes" : "no"); }
};
Out &operator<<(Out &o, const Pair &p) { return o << '(' << p.a << ',' << p.b << ')'; }

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int stream_test() {
    int r = 0;
    Out out;
    out << "x=" << 12 << ',' << -305 << ';';
    if (!str_eq(out.buf, "x=12,-305;")) r++;
    Pair pr; pr.a = 3; pr.b = 4;
    Out o2;
    o2 << "p" << pr << pr << '!';
    if (!str_eq(o2.buf, "p(3,4)(3,4)!")) r++;
    Out o3;
    o3 << nextval() << nextval() << nextval();      // left to right
    if (!str_eq(o3.buf, "123")) r++;
    Out o4;
    bool yes = 3 < 4;
    o4 << yes << ' ' << 0 << ' ' << (1 << 4) << ' ' << (1 + 2 << 1);    // built-in << inside is untouched
    if (!str_eq(o4.buf, "yes 0 16 6")) r++;
    return r;
}

// ---- && || and , : overloaded forms evaluate both operands, left first -------------------------------------------
int order = 0;
int evals = 0;
struct L {
    int v;
    L() : v(0) {}
    L(int x) : v(x) {}
    L operator&&(const L &o) const { return L(v != 0 && o.v != 0); }
    L operator||(const L &o) const { return L(v != 0 || o.v != 0); }
    int operator,(const L &o) const { return v * 100 + o.v; }
};
L mk(int v) { evals++; order = order * 10 + v; return L(v); }
int hit(int v) { evals++; order = order * 10 + v; return v; }

static int logic_test() {
    int r = 0;
    order = 0; evals = 0;
    L x = mk(0) && mk(1);                           // not short-circuited
    if (x.v != 0 || evals != 2 || order != 1) r++;  // order 0 then 1 -> 01 = 1
    order = 0; evals = 0;
    L y = mk(1) || mk(2);                           // both evaluated too
    if (y.v != 1 || evals != 2 || order != 12) r++;
    order = 0; evals = 0;
    L z = mk(3) || mk(0) && mk(4);                  // && binds tighter: 3 || (0 && 4), left to right
    if (z.v != 1 || evals != 3 || order != 304) r++;
    order = 0; evals = 0;
    int c = (mk(5), mk(6));                         // overloaded comma: both sides, left first
    if (c != 506 || evals != 2 || order != 56) r++;
    // built-in && || still short-circuit
    order = 0; evals = 0;
    int a1 = hit(0) && hit(1);
    int a2 = hit(1) || hit(2);
    if (a1 != 0 || a2 != 1 || evals != 2 || order != 1) r++;
    int cb = (hit(7), hit(8));
    if (cb != 8) r++;
    return r;
}

// ---- precedence: the same expression on a wrapper class and on ints ----------------------------------------------
struct I {
    int v;
    I() : v(0) {}
    I(int x) : v(x) {}
};
I operator+(I a, I b) { return I(a.v + b.v); }
I operator-(I a, I b) { return I(a.v - b.v); }
I operator*(I a, I b) { return I(a.v * b.v); }
I operator%(I a, I b) { return I(a.v % b.v); }
I operator<<(I a, I b) { return I(a.v << b.v); }
I operator>>(I a, I b) { return I(a.v >> b.v); }
I operator&(I a, bool b) { return I(a.v & (b ? 1 : 0)); }          // `B == C` is a bool
I operator&(I a, I b) { return I(a.v & b.v); }
I operator|(I a, I b) { return I(a.v | b.v); }
I operator^(I a, I b) { return I(a.v ^ b.v); }
bool operator==(I a, I b) { return a.v == b.v; }
bool operator<(I a, I b) { return a.v < b.v; }

static int prec_test() {
    int r = 0;
    int a = 5, b = 2, c = 3, d = 12;
    I A(5), B(2), C(3), D(12);
    if ((A << B + C).v != (a << b + c)) r++;
    if ((A + B << C).v != (a + b << c)) r++;
    if ((D >> B - C + A).v != (d >> b - c + a)) r++;
    if ((A & B | C).v != (a & b | c)) r++;
    if ((A | B & C).v != (a | b & c)) r++;
    if ((A ^ B & C | D).v != (a ^ b & c | d)) r++;
    if ((A | B ^ C).v != (a | b ^ c)) r++;
    if ((D % C * B).v != (d % c * b)) r++;
    if ((D * C % B + A).v != (d * c % b + a)) r++;
    if ((A & B == C).v != (a & b == c)) r++;          // A & (B == C)
    if ((D & B << A).v != (d & b << a)) r++;
    if ((A << B < C) != (a << b < c)) r++;
    I t = A + B * C << B;
    if (t.v != (a + b * c << b)) r++;
    I u = D - A - B;                                  // left associative
    if (u.v != d - a - b) r++;
    I w = D >> B >> B;
    if (w.v != (d >> b >> b)) r++;
    return r;
}

// ---- a pointer return, and a class used as an operand of a template-free generic array -------------------------------
class Arr {
public:
    int data[4];
    Arr() { data[0] = 1; data[1] = 2; data[2] = 3; data[3] = 4; }
    int *operator%(int i) { return &data[i]; }
    int operator|(int i) const { return data[i]; }
};

static int arr_test() {
    int r = 0;
    Arr a;
    *(a % 2) = 30;
    if ((a | 2) != 30 || (a | 1) != 2) r++;
    int *p = a % 3;
    *p += 5;
    if (a.data[3] != 9) r++;
    return r;
}

int main() {
    e = 0;
    e += bits_test();
    e += fixed_test();
    e += ns_test();
    e += ptr_test();
    e += stream_test();
    e += logic_test();
    e += prec_test();
    e += arr_test();
    test_errors = e;
    return 0;
}
