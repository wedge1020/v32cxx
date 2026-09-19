// Exercises operator overload declarator syntax: binary operators and
// subscript as class members, a unary operator sharing the SAME token
// ('-') as a binary overload of the same name (disambiguated purely by
// parameter count -- no special grammar needed for that), out-of-line
// operator definitions, and a free-function operator at top level.
//
// Vector2D's own operators take their other operand by const reference,
// not by value -- confirmed directly by Matthew: Vircon32's own hardware
// requires every parameter to be exactly one word (32 bits); a two-field
// struct like Vector2D is two words, so a pointer/reference is required
// (see docs/VIRCON32_QUIRKS.md's own entry #11). The RETURN type used to
// be an identical, harder problem this project couldn't fix at all: no
// function anywhere in this grammar could return a pointer or reference
// type (`int *getPtr()`/`int &getRef()` both failed to parse). That gap
// is now closed (VIRCON32_QUIRKS.md entry #12), so every value-producing
// operator below returns Vector2D BY POINTER instead, avoiding the
// word-size warning a by-value two-word return would still trigger.
// operator== and operator[] need no return-side change, since bool and
// int are already exactly one word.
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
        Vector2D *operator+(const Vector2D &other);
        Vector2D *operator-(const Vector2D &other);   // binary subtraction: 1 param
        Vector2D *operator-();                          // unary negation: 0 params
        bool operator==(const Vector2D &other);
        int operator[](int index);
        int x;
        int y;
};

Vector2D *Vector2D::operator+(const Vector2D &other) {
    Vector2D *result = new Vector2D(x + other.x, y + other.y);
    return result;
}

Vector2D *Vector2D::operator-(const Vector2D &other) {
    Vector2D *result = new Vector2D(x - other.x, y - other.y);
    return result;
}

Vector2D *Vector2D::operator-() {
    Vector2D *result = new Vector2D(-x, -y);
    return result;
}

bool Vector2D::operator==(const Vector2D &other) {
    return x == other.x && y == other.y;
}

// Free-function operator overload -- exercises func_header's func_name
// swap-in at top level, not just inside a class body.
Vector2D *operator*(const Vector2D &v, int scalar) {
    Vector2D *result = new Vector2D(v.x * scalar, v.y * scalar);
    return result;
}
