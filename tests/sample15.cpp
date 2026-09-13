// Exercises operator-overload-to-function-call rewriting (lowering phase
// 4): natural operator syntax (a + b, a == b, v[i]) gets resolved and
// rewritten into the equivalent explicit call, reusing phase 3's
// dispatch logic entirely. A free-function operator (operator*) is also
// exercised, confirming the non-member fallback path.

class Vector2D {
    public:
        Vector2D(int x, int y);
        Vector2D operator+(Vector2D other);
        bool operator==(Vector2D other);
        int operator[](int index);
    private:
        int x;
        int y;
};

Vector2D Vector2D::operator+(Vector2D other) {
    return other;
}

bool Vector2D::operator==(Vector2D other) {
    return true;
}

Vector2D addThem(Vector2D a, Vector2D b) {
    Vector2D sum = a + b;      // member operator+ via natural syntax
    return sum;
}

bool checkEqual(Vector2D a, Vector2D b) {
    return a == b;              // member operator== via natural syntax
}

int getElement(Vector2D v, int i) {
    return v[i];                 // member operator[] via natural syntax
}

Vector2D operator*(Vector2D v, int factor) {
    return v;
}

Vector2D scaleIt(Vector2D v) {
    return v * 2;                // free operator* via natural syntax
}
