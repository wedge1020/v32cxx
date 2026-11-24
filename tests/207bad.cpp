// Deliberately invalid: a qualified definition needs a declaration in that namespace (or class) that matches it.
namespace a {
    int f(int x);
    struct C { void m(); };
    namespace in { int g(int x); }
}
int a::zz() { return 1; }                 // line 7: a declares no zz
void a::C::nope() {}                      // line 8: C declares no nope
int a::f(float x) { return 2; }           // line 9: f exists, but not with this signature
int a::f(int x) { return 3; }             // fine
void a::C::m() {}                         // fine
int a::in::g(float x) { return 4; }       // line 12: a different overload, in a nested namespace
int a::in::g(int x) { return 5; }         // fine
int a::in::missing() { return 6; }        // line 14
int main() { return 0; }
