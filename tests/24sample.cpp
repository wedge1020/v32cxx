// Exercises vtable static instance population end to end (lower.c
// phase 8 + codegen.c's emit_vtable_instance) -- deliberately NOT
// reusing tests/sample7.cpp, sample12.cpp, or sample14.cpp, since NONE
// of those classes declare a constructor at all, which means phase 8
// (it only ever injects into an EXISTING constructor body) has nowhere
// to inject for any of them. Shape/Square here are the first classes in
// this project's test suite with both a virtual method AND a real
// constructor.
//
// Square::area overrides Shape::area, exercising emit_vtable_instance's
// cast logic too: Square_vtable_instance's "area" slot needs to store
// &Square__area__void (typed to take Square *) into a field declared to
// take Shape * (the canonical, originally-declaring class) -- the exact
// scenario this project's own reasoning-only, unconfirmed cast syntax
// exists for.

class Shape {
    public:
        Shape(int size);
        virtual int area();
    protected:
        int size;
};

Shape::Shape(int size) {
    this->size = size;
}

int Shape::area() {
    return size;
}

class Square : public Shape {
    public:
        Square(int side);
        int area();
};

Square::Square(int side) : Shape(side) {
}

int Square::area() {
    return size * size;
}

// main() constructs one of each via `new` -- deliberately not
// `Shape shape(4);` direct-stack-init-with-args, a syntax form this
// project's grammar support for isn't confirmed, and there's no reason
// to risk an unparseable test when `new`-based construction (allocation
// + constructor invocation, both already confirmed working end to end
// against the real compiler) is available and does exactly what's
// needed here -- exercises the full chain: allocation, construction,
// vtable population, and virtual dispatch, together, for the first time
// in this project's test suite.
//
// area()'s return value is deliberately discarded rather than assigned
// to a named local -- this test cares whether the calls compile and
// dispatch correctly, not about observing their results, and an
// assigned-but-never-read local would just be an unused-variable
// warning waiting to happen (confirmed: it was, before this fix).

void main() {
    Shape *shape = new Shape(4);
    Square *square = new Square(4);

    shape->area();
    square->area();
}
