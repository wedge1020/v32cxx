// Exercises operator overload declarator syntax: binary operators and
// subscript as class members, a unary operator sharing the SAME token
// ('-') as a binary overload of the same name (disambiguated purely by
// parameter count -- no special grammar needed for that), out-of-line
// operator definitions, and a free-function operator at top level.
//
// Expected in the sema dump: every operator gets a distinct,
// C-identifier-safe mangled name -- in particular, the two "operator-"
// overloads (unary negation, 0 params, vs. binary subtraction, 1 param)
// must mangle to DIFFERENT names despite sharing a name, the same way any
// other overloaded method would, or codegen would later try to emit two
// C functions with identical names. Both out-of-line definitions should
// attach to their in-class prototype exactly the way an ordinarily-named
// out-of-line method would.

class Vector2D {
    public:
        Vector2D(int x, int y);
        Vector2D operator+(Vector2D other);
        Vector2D operator-(Vector2D other);   // binary subtraction: 1 param
        Vector2D operator-();                  // unary negation: 0 params
        bool operator==(Vector2D other);
        int operator[](int index);
    private:
        int x;
        int y;
};

Vector2D Vector2D::operator+(Vector2D other) {
    return other;
}

Vector2D Vector2D::operator-(Vector2D other) {
    return other;
}

bool Vector2D::operator==(Vector2D other) {
    return true;
}

// Free-function operator overload -- exercises func_header's func_name
// swap-in at top level, not just inside a class body.
Vector2D operator*(Vector2D v, int scalar) {
    return v;
}
