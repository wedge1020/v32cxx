// Exercises a plain, C-style `struct` with no methods at all -- real
// C++'s own DEFAULT ACCESS for struct is public (the opposite of
// class's own default, private), so x/y here are directly accessible
// from outside Point without any explicit `public:` label at all --
// the CORE, distinguishing behavior this feature actually implements
// (everything else about struct/class is already shared, identical
// machinery). Also confirms no vtable/constructor overhead gets added
// just because this is written as `struct` instead of `class` -- a
// plain data struct with no methods should generate a plain C struct,
// nothing more (check the generated .c directly: no vtable pointer
// field, no allocator-with-constructor-call, just `struct Point { int
// x; int y; };`).

struct Point {
    int x;
    int y;
};

void main() {
    Point p;
    p.x = 3;
    p.y = 4;
}
