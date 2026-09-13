// Exercises new/delete with constructor arguments (lowering phase 6 +
// sema.c's resolve_new_expr): `new T(args)` now parses at all, gets its
// constructor overload resolved and arity-checked at sema time, and
// lowers to a placeholder allocator call with the args forwarded.

class Point {
    public:
        Point(int x, int y);
    private:
        int x;
        int y;
};

void makePoint() {
    Point *p = new Point(3, 4);
    delete p;
}
