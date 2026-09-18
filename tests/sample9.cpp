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
// (see docs/VIRCON32_QUIRKS.md's own entry #11). The RETURN type is a
// separate, harder problem this project can't yet fix: no function
// anywhere in this grammar can return a pointer or reference type at all
// (`int *getPtr()`/`int &getRef()` both fail to parse -- a real,
// separate, previously-undiscovered gap, not specific to operators),
// so a value-producing operator like operator+ still has to return
// Vector2D BY VALUE here and will still trigger this project's own
// word-size warning on that return -- left as a known, honestly-stated
// limitation rather than worked around, since there's no clean fix
// available yet. operator== and operator[] need no return-side change,
// since bool and int are already exactly one word.
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
        Vector2D operator+(const Vector2D &other);
        Vector2D operator-(const Vector2D &other);   // binary subtraction: 1 param
        Vector2D operator-();                          // unary negation: 0 params
        bool operator==(const Vector2D &other);
        int operator[](int index);
    private:
        int x;
        int y;
};

Vector2D Vector2D::operator+(const Vector2D &other) {
    return other;
}

Vector2D Vector2D::operator-(const Vector2D &other) {
    return other;
}

bool Vector2D::operator==(const Vector2D &other) {
    return true;
}

// Free-function operator overload -- exercises func_header's func_name
// swap-in at top level, not just inside a class body.
Vector2D operator*(const Vector2D &v, int scalar) {
    return v;
}
