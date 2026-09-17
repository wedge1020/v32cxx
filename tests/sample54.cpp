// Exercises C-style casts -- (Type)expr, (Type *)expr -- now reachable
// from user-written source, not just lowering-synthesized (lower.c has
// always inserted its own casts internally -- implicit upcasts,
// receiver casts -- but a user could never write one directly before
// this round).
//
// Also exercises a cast wrapping a BARE MEMBER REFERENCE and a cast
// wrapping a VIRTUAL CALL specifically -- two real, separate gaps
// found and fixed the same round this feature was added: neither
// this-injection (phase 2) nor call finalization (phase 3) originally
// reached INSIDE a user-written cast's own wrapped expression, since
// AST_CAST had only ever appeared as something a LATER lowering phase
// inserted, never as part of the original AST those earlier phases
// walk (see docs/DESIGN_NOTES.md for the full story).
//
// Expected: truncated = 3 (3.9 truncated toward zero); fromMember = 5
// (Shape::area()'s own "(int)size" -- a cast wrapping a bare member
// reference, confirming it correctly became "(int)this->size", not a
// bare, undeclared "size"); viaBase = 5 too (the SAME area(), called
// THROUGH a Shape* that actually points at a Square -- Square doesn't
// override area() here, so this also confirms the pointer-downcast-
// then-upcast round trip didn't corrupt anything).

class Shape {
    public:
        Shape(float startSize);
        virtual int area();
    protected:
        float size;
};

Shape::Shape(float startSize) {
    this->size = startSize;
}

int Shape::area() {
    return (int)size;
}

class Square : public Shape {
    public:
        Square(float side);
};

Square::Square(float side) : Shape(side) {
}

void main() {
    float f = 3.9f;
    int truncated = (int)f;

    Square *sq = new Square(5.0);
    int fromMember = sq->area();

    Shape *base = (Shape *)sq;
    int viaBase = (int)base->area();

    delete sq;
}
