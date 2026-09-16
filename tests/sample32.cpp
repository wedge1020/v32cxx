// Exercises virtual destructor dispatch: `delete basePtr;` through an
// ancestor-typed pointer should call the DERIVED destructor, not the
// ancestor's own -- only because Shape's destructor is declared
// `virtual` here. Shape/Square mirror tests/sample24.cpp's own
// hierarchy, but sample24's Shape had no destructor at all; this is the
// first test exercising a virtual destructor specifically.
//
// Also exercises a SEPARATE fix this same test surfaced, unrelated to
// destructor dispatch itself: `Shape *shapePtr = new Square(4);`
// assigns a derived pointer to a base-typed variable with no explicit
// cast (this project's grammar has no C-style cast expression at all).
// Confirmed directly that Vircon32 rejects this outright ("types are
// not compatible: cannot assign struct Square* to struct Shape*") --
// stricter than real C++, which allows the implicit upcast freely.
// Fixed by a new lowering phase (lower.c's insert_pointer_cast_stmt)
// that inserts an explicit cast to the declared type whenever a
// VarDecl's pointer-typed initializer resolves to a different,
// related class -- scoped narrowly to VarDecl initializers specifically
// (not plain assignment, function arguments, or return values, which
// could hit the identical mismatch but aren't covered here).

class Shape {
    public:
        Shape(int size);
        virtual ~Shape();
        virtual int area();
    protected:
        int size;
};

Shape::Shape(int size) {
    this->size = size;
}

Shape::~Shape() {
    this->size = 0;
}

int Shape::area() {
    return size;
}

class Square : public Shape {
    public:
        Square(int side);
        ~Square();
        int area();
};

Square::Square(int side) : Shape(side) {
}

Square::~Square() {
    this->size = -1;
}

int Square::area() {
    return size * size;
}

void main() {
    Shape *shapePtr = new Square(4);
    shapePtr->area();
    delete shapePtr;
}
