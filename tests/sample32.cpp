// Exercises virtual destructor dispatch: `delete basePtr;` through an
// ancestor-typed pointer should call the DERIVED destructor, not the
// ancestor's own -- only because Shape's destructor is declared
// `virtual` here. Shape/Square mirror tests/sample24.cpp's own
// hierarchy, but sample24's Shape had no destructor at all; this is the
// first test exercising a virtual destructor specifically.
//
// A separate, genuinely uncertain question this test ALSO exercises,
// unrelated to destructor dispatch itself: `Shape *shapePtr =
// new Square(4);` assigns a derived pointer to a base-typed variable
// with no explicit cast (this project's grammar has no C-style cast
// expression at all -- confirmed directly, nothing in parser.y produces
// one from source; AST_CAST only ever comes from lowering's own
// receiver-cast insertion). If Vircon32 rejects this the way it's
// already shown itself strict about other pointer-type mismatches, that
// would be a real, separate, pre-existing gap (no cast-insertion for an
// ordinary assignment, only for a method call's receiver) -- not a
// problem with the virtual-destructor-dispatch fix this test is
// actually here to confirm.

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

Square::Square(int side) {
    this->size = side;
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
