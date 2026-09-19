// Exercises member-initializer DECLARATION order, not WRITTEN order --
// the well-known C++ rule (and footgun): members initialize in the
// order they're DECLARED in the class, regardless of the order they
// appear in the initializer list itself.
//
// Fields are declared y-then-x here, but the list below writes x(a),
// y(b) -- x FIRST. The correct generated assignment order is still
// y-then-x (declaration order): "this->y = b; this->x = a;", NOT the
// list's own left-to-right "this->x = a; this->y = b;". Check the
// generated .c directly to confirm the order, not just that it
// compiles.

class Pair {
    public:
        Pair(int a, int b);
    private:
        int y;
        int x;
};

Pair::Pair(int a, int b) : x(a), y(b) {
}

void main() {
    Pair *p = new Pair(1, 2);
    delete p;
}
