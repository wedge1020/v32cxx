// Exercises direct-initialization with constructor arguments on a
// stack-allocated local -- `Shape shape(7);` -- a real, previously
// flagged gap (README.md's own "What doesn't exist yet" list) now
// closed. Only two forms existed before this: an explicit `=`
// initializer, or no initializer at all (triggering this project's own
// zero-argument-constructor injection); this grammar had no shape for
// constructor arguments in parens on a plain declarator at all.
//
// Covers three things together:
//   1. A single-argument constructor call (`Shape shape(7);`),
//      confirming the receiver (&shape) and the argument both reach the
//      generated constructor call correctly.
//   2. A multi-argument constructor call (`Rect r(3, 4);`), confirming
//      more than one argument threads through in order.
//   3. Overload resolution still working when reached through THIS
//      path, not just through `new T(args)` -- Rect has two
//      constructors (one- and two-argument), and sema.c's
//      resolve_new_expr (reused unchanged for AST_DIRECT_INIT) has to
//      pick the right one from arg count/type just like it already
//      does for `new`.
//
// Expected: shapeArea = 49 (7*7); rectArea = 12 (3*4); squareArea = 25
// (5*5, exercising the one-argument Rect overload specifically, so
// both overloads are actually reached by this test, not just declared).

class Shape {
    public:
        Shape(int size);
        int area();
    private:
        int size;
};

Shape::Shape(int size) {
    this->size = size;
}

int Shape::area() {
    return size * size;
}

class Rect {
    public:
        Rect(int side);
        Rect(int w, int h);
        int area();
    private:
        int width;
        int height;
};

Rect::Rect(int side) {
    this->width = side;
    this->height = side;
}

Rect::Rect(int w, int h) {
    this->width = w;
    this->height = h;
}

int Rect::area() {
    return width * height;
}

void main() {
    Shape shape(7);
    int shapeArea = shape.area();

    Rect r(3, 4);
    int rectArea = r.area();

    Rect square(5);
    int squareArea = square.area();
}
