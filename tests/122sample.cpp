// *****************************************************************************
//  tests/122sample.cpp — operator++ / operator--, prefix and postfix
//
//  Prefix is `operator++()`, postfix `operator++(int)` with C++'s dummy
//  int parameter; members and free functions; built-in ++ on ints is left
//  alone. Also an OBJECT declared in a for loop's init clause
//  (`for (Counter it; ...; ++it)`), which is now constructed before the
//  loop and destroyed after it.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int for_init_errors();
class Counter
{
    public:
        int n;
        Counter() { n = 0; }
        Counter &operator++() { n++; return *this; }            // ++c
        Counter operator++(int) { Counter old = *this; n++; return old; }   // c++
        Counter &operator--() { n--; return *this; }
        Counter operator--(int) { Counter old = *this; n--; return old; }
};
class Big { public: int a; int b; Big() { a = 0; b = 0; } Big &operator++() { a++; b += 2; return *this; } };
class Wrap { public: int v; };
Wrap &operator++(Wrap &w) { w.v += 10; return w; }
Wrap operator++(Wrap &w, int) { Wrap old = w; w.v += 100; return old; }
int main()
{
    int e = 0;
    Counter c;
    ++c;
    c++;
    if (c.n != 2) e++;
    Counter old = c++;
    if (old.n != 2 || c.n != 3) e++;
    Counter &r = ++c;
    if (r.n != 4 || (++c).n != 5) e++;
    --c; c--;
    if (c.n != 3) e++;
    Counter before = c--;
    if (before.n != 3 || c.n != 2) e++;
    Big b; ++b; ++b;
    if (b.a != 2 || b.b != 4) e++;
    Wrap w; w.v = 0;
    ++w;
    Wrap pw = w++;
    if (pw.v != 10 || w.v != 110) e++;
    int i = 0; i++; ++i; i--;           // built-ins untouched
    for (int k = 0; k < 3; k++) i++;
    if (i != 4) e++;
    for (Counter it; it.n < 5; ++it) i++;
    if (i != 9) e++;
    e += for_init_errors();
    test_errors = e;
    return 0;
}

// for-init objects get their constructor and destructor
int g_made = 0;
int g_gone = 0;
class It
{
    public:
        int n;
        int lim;
        It(int l) { n = 0; lim = l; g_made++; }
        ~It() { g_gone++; }
        bool ok() { return n < lim; }
        It &operator++() { n++; return *this; }
};
int for_init_errors()
{
    int e = 0;
    int total = 0;
    for (It it(4); it.ok(); ++it) total += it.n;
    if (total != 6 || g_made != 1 || g_gone != 1) e++;
    for (It a(2); a.ok(); ++a) { if (a.n == 1) break; total++; }
    if (g_gone != 2 || total != 7) e++;
    return e;
}
