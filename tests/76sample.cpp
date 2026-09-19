// Exercises a copy constructor -- `Shape(const Shape &other);` -- one
// of the most common patterns in intro-course OOP, and a real,
// previously-undiscovered bug found by an audit specifically looking
// for gaps a student learning C++ might hit early, not by a failing
// existing test: this never actually resolved at all once a class had
// more than one constructor (an unambiguous, non-overloaded
// constructor never needed full argument-type matching -- arity alone
// was enough -- so this bug only became visible the moment a class
// declared BOTH an ordinary constructor and a copy constructor
// together, which no prior test in this suite had done).
//
// Root cause: overload resolution's type-matching used plain type
// equality, which requires identical AST shapes on both sides -- a
// `const Shape &` parameter is a reference wrapping Shape, while a
// plain `Shape` argument is bare Shape, different shapes even though
// real C++ freely binds a value to a const-reference parameter (the
// entire mechanism a copy constructor depends on to receive its own
// argument). Fixed by a new type_matches_param helper (sema.c) that
// falls back to comparing the reference's own REFERENT type when the
// exact-equality check fails and the parameter side is a (const)
// reference.
//
// Exercised through BOTH constructor-invocation paths this project
// supports -- `new Shape(a)` and this round's own new direct-init
// syntax, `Shape c(a);` -- confirming the fix isn't specific to
// either one (type_matches_param is shared by resolve_overload_generic,
// which both resolve_new_expr and AST_DIRECT_INIT's own resolution
// go through identically).
//
// Expected: aArea = 25 (5*5); bArea = 25 (copy of a, via `new`); cArea
// = 25 (copy of a, via direct-init) -- all three equal, confirming the
// copy actually copied the right value in every path.

class Shape {
    public:
        Shape(int size);
        Shape(const Shape &other);
        int area();
    private:
        int size;
};

Shape::Shape(int size) {
    this->size = size;
}

Shape::Shape(const Shape &other) {
    this->size = other.size;
}

int Shape::area() {
    return size * size;
}

void main() {
    Shape a(5);
    int aArea = a.area();

    Shape *b = new Shape(a);
    int bArea = b->area();

    Shape c(a);
    int cArea = c.area();
}
