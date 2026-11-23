// Deliberately invalid: an array MEMBER of class type with a brace initializer.
// The list used to be dropped without a word (the elements were only
// default-constructed); v32c++ now says it is not supported. A local or global
// array takes the same list (tests/174sample.cpp).
class Counter { public: int id; Counter(int i) { id = i; } };
class Holder {
public:
    Counter m[2] = { { 1 }, { 2 } };
};
int main(void) { Holder h; return 0; }
