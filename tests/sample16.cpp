// Exercises reference-to-pointer rewriting (lowering phase 5): a
// reference parameter's `.` access should become `->` and its declared
// type should become a pointer, while a plain by-value parameter's `.`
// access stays `.`, completely unaffected.
//
// (value is public here specifically to keep this test isolated to
// reference lowering -- private access from a free function is a
// SEPARATE concern, already covered by tests/sample10.cpp.)

class Counter {
    public:
        Counter(int start);
        int value;
};

void bumpByReference(Counter& c) {
    c.value = c.value + 1;   // reference: "." should become "->"
}

void bumpByValue(Counter c) {
    c.value = c.value + 1;   // plain value: "." should stay "."
}
