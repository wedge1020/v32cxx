// Exercises per-element constructor invocation for a stack array of
// class objects -- `Shape shapes[3];` -- previously the most dangerous
// documented gap in this project (README's own words: "SILENT, not a
// rejection... nothing about it looks wrong until the program runs").
// Before this round, this parsed and transpiled clean, but the
// generated code was just `struct Shape shapes[3];` with no
// constructor call anywhere -- every element left as genuinely
// uninitialized stack memory if Shape had a real constructor body,
// same failure shape tests/22sample.cpp's own comment describes for
// the single-object (non-array) case phase 7 originally closed.
//
// Fixed by extending that SAME phase (inject_ctor_calls_block, lower.c)
// with one more shape to build -- a `for` loop over the array calling
// the element class's own zero-argument constructor on `&arr[i]` for
// each index -- reusing AST_FOR/AST_SUBSCRIPT/`post++`, all already
// used elsewhere in this project's own lowered output, rather than
// inventing anything new.
//
// Expected: 100, 100, 100 -- all three elements actually constructed
// (each with size=10, area=100), not left as uninitialized garbage
// that would print something different (or crash) on a real run.

class Shape {
    public:
        Shape();
        int area();
    private:
        int size;
};

Shape::Shape() {
    this->size = 10;
}

int Shape::area() {
    return size * size;
}

void main() {
    Shape shapes[3];

    int area0 = shapes[0].area();
    int area1 = shapes[1].area();
    int area2 = shapes[2].area();
}
