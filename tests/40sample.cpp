// Exercises base-class delegation AND a primitive member-field
// initializer together, in the SAME member-initializer list --
// confirms the full insertion order this project builds: base-
// constructor call first, then member-field assignments, then the
// original constructor body. Square's own `label` is private, same
// "no other legal way to set it" motivation as the rest of this
// feature's own tests.
//
// Expected: area() returns 16 (4*4, via Shape's own delegated
// constructor), getLabel() returns 99 (via the member initializer) --
// proving both mechanisms fired correctly in the same constructor.

class Shape {
    public:
        Shape(int startSize);
        int area();
    private:
        int size;
};

Shape::Shape(int startSize) {
    this->size = startSize;
}

int Shape::area() {
    return size * size;
}

class Square : public Shape {
    public:
        Square(int side, int labelValue);
        int getLabel();
    private:
        int label;
};

Square::Square(int side, int labelValue) : Shape(side), label(labelValue) {
}

int Square::getLabel() {
    return label;
}

void main() {
    Square *sq = new Square(4, 99);
    int a = sq->area();
    int l = sq->getLabel();
    delete sq;
}
