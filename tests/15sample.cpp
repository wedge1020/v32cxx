// Exercises operator-overload-to-function-call rewriting (lowering phase
// 4): natural operator syntax (a + b, a == b, v[i]) gets resolved and
// rewritten into the equivalent explicit call, reusing phase 3's
// dispatch logic entirely. A free-function operator (operator*) is also
// exercised, confirming the non-member fallback path.
//
// Vector2D's own operators take their other operand by const reference,
// and every free function below that takes a Vector2D also takes it by
// const reference -- confirmed directly by Matthew: Vircon32's own
// hardware requires every parameter to be exactly one word (32 bits); a
// two-field struct like Vector2D is two words (see docs/VIRCON32_
// QUIRKS.md's own entry #11). The RETURN type used to be an identical,
// harder problem this project couldn't fix at all: no function anywhere
// in this grammar could return a pointer or reference type. That gap is
// now closed (VIRCON32_QUIRKS.md entry #12), so operator+ itself, and
// every free function below that hands back a Vector2D (addThem,
// scaleIt), now return Vector2D BY POINTER instead, avoiding the
// word-size warning a by-value two-word return would still trigger.
// checkEqual/getElement need no return-side change, since bool and int
// are already exactly one word.

class Vector2D {
    public:
        Vector2D(int x, int y);
        Vector2D *operator+(const Vector2D &other);
        bool operator==(const Vector2D &other);
        int operator[](int index);
        int x;
        int y;
};

Vector2D *Vector2D::operator+(const Vector2D &other) {
    Vector2D *result = new Vector2D(x + other.x, y + other.y);
    return result;
}

bool Vector2D::operator==(const Vector2D &other) {
    return true;
}

Vector2D *addThem(const Vector2D &a, const Vector2D &b) {
    Vector2D *sum = a + b;      // member operator+ via natural syntax
    return sum;
}

bool checkEqual(const Vector2D &a, const Vector2D &b) {
    return a == b;              // member operator== via natural syntax
}

int getElement(const Vector2D &v, int i) {
    return v[i];                 // member operator[] via natural syntax
}

Vector2D *operator*(const Vector2D &v, int factor) {
    Vector2D *result = new Vector2D(v.x * factor, v.y * factor);
    return result;
}

Vector2D *scaleIt(const Vector2D &v) {
    return v * 2;                // free operator* via natural syntax
}
