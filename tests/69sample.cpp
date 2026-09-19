// Exercises `const` member functions -- an in-class declaration
// (func_header's own new opt_const) paired with an out-of-line
// definition (out_of_line_def's own separate, identical addition),
// both with a trailing `const` after the parameter list. Confirms
// this project doesn't just parse the keyword but actually propagates
// it to the generated C: the injected `this` parameter for a const
// method becomes `const Rectangle *this`, not plain `Rectangle
// *this` -- see this_inject_method's own comment in lower.c. This
// project still does NOT enforce const-correctness (no error for
// writing through `this` inside a const method) -- see
// AST_FUNC_DECL's own doc comment in ast.h for the full, deliberate
// boundary.
//
// Rectangle is default-constructed then has its own public `width`/
// `height` fields set directly, NOT constructed as `Rectangle r(3,
// 4);` -- this project has no grammar support for direct-
// initialization with constructor arguments on a stack-allocated
// local at all (opt_initializer only ever accepts `= expr` or
// nothing; inject_ctor_calls_block, lower.c, only ever looks up a
// ZERO-argument constructor), a real, separate, pre-existing gap this
// test's own earlier version incorrectly assumed was supported.
//
// Expected: area = 12 (a 3x4 rectangle, read through a const method);
// area2 = 20 (a 4x5 rectangle, same const method).

class Rectangle {
    public:
        Rectangle();
        int area() const;
        int width;
        int height;
};

Rectangle::Rectangle() {
    this->width = 0;
    this->height = 0;
}

int Rectangle::area() const {
    return width * height;
}

void main() {
    Rectangle r;
    r.width = 3;
    r.height = 4;
    int area = r.area();

    Rectangle r2;
    r2.width = 4;
    r2.height = 5;
    int area2 = r2.area();
}

