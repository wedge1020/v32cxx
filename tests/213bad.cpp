// Unary operator& (and a free or parameterized operator->) cannot be overloaded.
struct P { int v; };
class A { public: int v; A *operator&() { return this; } };
P *operator&(P &p) { return &p; }
class B { public: int v; B *operator->(int k) { return this; } };
struct Q { int v; };
Q *operator->(Q &q) { return &q; }
class D { public: int v; D operator&(const D &o) const { return *this; } };    // binary: fine
int main() { return 0; }
