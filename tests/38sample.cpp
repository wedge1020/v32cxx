// A member-initializer list naming a data member whose own type IS a
// class (not a pointer/reference to one): `: thing(42)` constructs the
// member with its own constructor. Written as a deliberately INVALID
// sample when this wasn't supported (it expected a "class-typed fields
// aren't supported yet" error); since class-typed member initializers
// landed (tests/112sample.cpp) it is an ordinary, valid program, and
// `make test` checks that it transpiles (20261009: found still listed
// as an expected failure, which `make test`'s `-` prefix had hidden).
class Inner {
    public:
        Inner(int v);
    private:
        int value;
};

Inner::Inner(int v) {
    this->value = v;
}

class Outer {
    public:
        Outer(int v);
    private:
        Inner thing;
};

Outer::Outer(int v) : thing(v) {
}

void main() {
}
