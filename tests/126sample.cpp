// *****************************************************************************
//  tests/126sample.cpp — conversion operators
//
//  `operator int() const`, `operator bool()`, `operator int *()`,
//  `operator float()`, one defined out of line (`Handle::operator bool()
//  const`), one inherited. Each is called wherever the object is used as a
//  value of that type: an initializer, `=` and `+=`, a return, if / while /
//  ?: / ! / && / ||, a C-style cast, static_cast, an argument (also when it
//  decides between overloads), arithmetic and comparison, an array index.
//  An int conversion also serves a float or bool target when it is the
//  class's only arithmetic one.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

class Counter
{
  public:
    Counter(int start) : value(start) {}
    operator int() const { return value; }
    void bump() { value++; }
  private:
    int value;
};

class Handle
{
  public:
    Handle(int *p) : ptr(p) {}
    explicit operator bool() const;
    operator int *() const { return ptr; }
  private:
    int *ptr;
};

Handle::operator bool() const { return ptr != nullptr; }

class Meters
{
  public:
    Meters(float m) : m(m) {}
    operator float() const { return m; }
  private:
    float m;
};

class Base { public: Base() : id(7) {} operator int() const { return id; } int id; };
class Derived : public Base { public: Derived() {} };

int twice(int x) { return x * 2; }
float halve(float f) { return f / 2.0; }
int pick(int x) { return 1; }
int pick(const char *s) { return 2; }
int as_int(const Counter &c) { return c; }
int ret_conv() { Counter c(41); c.bump(); return c; }

int main(void)
{
    int e = 0;
    Counter c(5);
    int n = c;                     if (n != 5) e++;
    c.bump();
    n = c;                         if (n != 6) e++;
    n += c;                        if (n != 12) e++;
    if (twice(c) != 12) e++;
    if (c + 1 != 7) e++;
    if (1 + c != 7) e++;
    if (c < 6 || c > 6) e++;
    if ((int)c != 6) e++;
    if (static_cast<int>(c) != 6) e++;
    if (!c) e++;
    if (c) {} else e++;
    if (pick(c) != 1) e++;
    if (as_int(c) != 6) e++;
    if (ret_conv() != 42) e++;
    int table[10];
    for (int i = 0; i < 10; i++) table[i] = i * 10;
    if (table[c] != 60) e++;
    int k = 0;
    while (k < c) k++;
    if (k != 6) e++;
    int t = c ? 1 : 0;             if (t != 1) e++;

    int x = 3;
    Handle h(&x), z(nullptr);
    if (!h) e++;
    if (z) e++;
    if (h && z) e++;
    if (!(h || z)) e++;
    int *p = h;                    if (p != &x || *p != 3) e++;
    if (static_cast<bool>(z)) e++;

    Meters m(3.0);
    float f = m;                   if (f != 3.0) e++;
    if (halve(m) != 1.5) e++;
    if (m * 2.0 != 6.0) e++;

    Derived d;
    int di = d;                    if (di != 7) e++;
    Derived *dp = &d;
    if (*dp != 7) e++;

    test_errors = e;
    return 0;
}
