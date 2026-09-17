// Deliberately invalid: a `class` (not `struct`) with a data member
// accessed directly from outside, with NO explicit `public:` label at
// all -- class's own default access is still private, completely
// unaffected by struct's own default-public behavior (see
// sample51.cpp). Should produce exactly one semantic error -- confirms
// the two keywords' own defaults are genuinely independent, not
// accidentally conflated by sharing the same underlying machinery.

class Point {
    int x;
};

void main() {
    Point p;
    p.x = 3;
}
