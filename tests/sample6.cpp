// Exercises parameter-type-aware mangling and overload-aware out-of-line
// attachment. Vector has TWO constructors and TWO overloads of resize(),
// distinguished only by parameter count/types (both constructors share
// the name "Vector", both resize()s share the name "resize" -- name alone
// can't tell them apart).
//
// Before the overload-matching fix, attach_out_of_line() matched by name
// only, so Vector::Vector(int size)'s out-of-line body would have been
// attached to whichever "Vector" prototype came first in the class body
// (the no-arg constructor) -- silently wrong, and leaving the real no-arg
// constructor sitting there with a mismatched body. Confirms:
//   - each overload gets a DISTINCT mangled name (check the sema dump:
//     no two entries should collide, and the two "Vector" entries and two
//     "resize" entries should differ only in mangled name)
//   - every out-of-line definition below attaches to the RIGHT prototype,
//     not just "the first same-named one"

class Vector {
    public:
        Vector();
        Vector(int size);
        void resize(int newSize);
        void resize(int newSize, int fillValue);
    private:
        int length;
};

Vector::Vector() {
    length = 0;
}

Vector::Vector(int size) {
    length = size;
}

void Vector::resize(int newSize) {
    length = newSize;
}

void Vector::resize(int newSize, int fillValue) {
    length = newSize;
}
