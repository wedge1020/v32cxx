// Exercises dynamic_cast -- accepted (parses, transpiles as a plain
// cast, exactly like static_cast), but should produce exactly ONE
// warning (not an error -- this does not stop the transpile) via
// sema_warning, confirming this project is honest about the gap: no
// actual RTTI-backed runtime check happens, unlike what real
// dynamic_cast promises. Should still generate a working .c file with
// exit code 0 -- a warning, unlike a semantic error, never causes
// main.c to report failure (see sema_get_warning_count's own doc
// comment in sema.h).

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
};

Square::Square(int side) : Shape(side) {
}

void main() {
    Square *sq = new Square(4);
    Shape *base = dynamic_cast<Shape *>(sq);
    int a = base->area();
    delete sq;
}
