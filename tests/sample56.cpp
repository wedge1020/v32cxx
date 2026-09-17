// Exercises static_cast/const_cast/reinterpret_cast -- all three
// collapse to the exact same generated C as a plain C-style cast (see
// AST_CAST's own doc comment in ast.h), confirmed here across a
// primitive cast, a pointer upcast through a class hierarchy, and a
// virtual call dispatched through the result of one.
//
// Expected: truncated = 3 (static_cast<int> truncation, same as a
// C-style cast would give -- see sample54.cpp); viaBase = 4 (Square's
// own area(), reached through a reinterpret_cast to Shape* and then a
// const_cast back to Shape* -- confirms neither cast interferes with
// virtual dispatch any differently than a C-style cast already didn't).

class Shape {
    public:
        Shape(int s);
        virtual int area();
    protected:
        int size;
};

Shape::Shape(int s) {
    this->size = s;
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
    return size;
}

void main() {
    float f = 3.9;
    int truncated = static_cast<int>(f);

    Square *sq = new Square(4);
    Shape *base = reinterpret_cast<Shape *>(sq);
    Shape *base2 = const_cast<Shape *>(base);
    int viaBase = base2->area();

    delete sq;
}
