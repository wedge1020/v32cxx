// Deliberately invalid: wrong number of constructor arguments to `new`.
// Exercises sema.c's resolve_new_expr correctly diagnosing this instead
// of silently accepting it -- the diagnostic gap this round closed.

class Point {
    public:
        Point(int x, int y);
    private:
        int x;
        int y;
};

void makePoint() {
    Point *p = new Point(3);  // ERROR: Point's constructor takes 2 args, not 1
}
