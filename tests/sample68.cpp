// Exercises `const` as a type prefix -- variables, function
// parameters, pointee-const through a pointer, and a class-typed const
// reference parameter (confirming type_to_class's own new
// AST_CONST_TYPE case correctly resolves a method call through a
// const-qualified class parameter, and that type_signature_str's own
// new case correctly mangles it -- both real, separate fixes this
// round needed beyond just parsing the syntax). This project accepts
// the syntax and emits it correctly in generated C, but does NOT
// enforce const-correctness anywhere (no error for reassigning a
// const variable, no error for calling a non-const method through a
// const reference) -- see AST_CONST_TYPE's own doc comment in ast.h
// for the full, deliberate scope boundary.
//
// Shape is default-constructed then has its own public `size` field
// set directly (`shape.size = 7;`), NOT constructed as `Shape
// shape(7);` -- this project has no grammar support for direct-
// initialization with constructor arguments on a stack-allocated
// local at all (opt_initializer only ever accepts `= expr` or
// nothing; inject_ctor_calls_block, lower.c, only ever looks up a
// ZERO-argument constructor), a real, separate, pre-existing gap this
// test's own earlier version incorrectly assumed was supported.
//
// Expected: sum = 8 (addConst(5, 3), two const int parameters);
// value = 42 (read through a pointer-to-const-int); classValue = 7
// (getArea, taking a const Shape&, confirming call resolution through
// a const-qualified class parameter still works correctly).

int addConst(const int a, const int b) {
    return a + b;
}

int readThroughConstPtr(const int *p) {
    return *p;
}

class Shape {
    public:
        Shape();
        int area();
        int size;
};

Shape::Shape() {
    this->size = 0;
}

int Shape::area() {
    return size;
}

int getArea(const Shape &s) {
    return s.area();
}

void main() {
    const int x = 5;
    const int y = 3;
    int sum = addConst(x, y);

    const int myValue = 42;
    int value = readThroughConstPtr(&myValue);

    Shape shape;
    shape.size = 7;
    int classValue = getArea(shape);
}

