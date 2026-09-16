// Exercises base-class constructor delegation (": Base(args)") -- the
// specific case that makes this a real FUNCTIONAL gap, not just a style
// preference: Shape's own `size` is PRIVATE, so Square's constructor has
// no legal way to reach it directly at all (a derived class can reach an
// inherited PROTECTED or PUBLIC member, but never a PRIVATE one).
// Delegating to Shape's own constructor is the only correct way to
// initialize it -- before this round, there was no way to write this at
// all, and Square's own `size` would have had to become protected just
// to work around the gap, defeating the point of encapsulation.
//
// Expected: area() returns 16 (4*4) -- proving `size` was actually set
// through Shape's own constructor, not left as stack/heap garbage.

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
        Square(int side);
};

Square::Square(int side) : Shape(side) {
}

void main() {
    Square *sq = new Square(4);
    int a = sq->area();
    delete sq;
}
