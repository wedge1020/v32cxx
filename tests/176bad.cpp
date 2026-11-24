// Deliberately invalid: a list of values for an array member whose elements are
// plain structs (no constructor to call). Only an array of objects whose class
// has a constructor can be initialized from `m{...}` (tests/176sample.cpp).
class Pt { public: int v; };
class Holder {
public:
    Pt m[2];
    Holder() : m{ { 1 }, { 2 } } { }
};
int main(void) { Holder h; return 0; }
