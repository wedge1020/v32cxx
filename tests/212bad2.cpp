// leading `::`: a rooted name is looked up in the file scope only
namespace a { int f(int v) { return v; } namespace in { int g(int v) { return v; } } }
int main() {
    int r = ::a::f(1);          // fine
    r += ::a::in::g(1);         // fine
    r += ::a::g(1);             // no g in a
    r += ::in::g(1);            // no in at file scope
    return r;
}
