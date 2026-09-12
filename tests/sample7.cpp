// Exercises vtable slot assignment. Shape declares three virtual members
// (a virtual destructor, area(), and draw()); Circle overrides draw()
// WITHOUT repeating the 'virtual' keyword (real C++ still treats this as
// an override -- sema.c is expected to mark it virtual anyway), leaves
// area() untouched, and introduces one brand-new virtual method, glow(),
// of its own.
//
// Expected in the sema dump:
//   - Shape's vtable has 3 slots: ~Shape (dtor), area, draw -- each
//     pointing at Shape's own implementation.
//   - Circle's vtable has 4 slots: the SAME ~Shape and area slots
//     inherited unchanged (still pointing at Shape's implementations,
//     since Circle doesn't touch either), draw REPOINTED at Circle's own
//     implementation (same slot index as Shape's), and one new slot for
//     glow appended after.
//   - Circle's draw() shows up tagged [virtual] in the methods list even
//     though the keyword isn't on this declaration.

class Shape {
    public:
        virtual ~Shape();
        virtual int area();
        virtual void draw();
    private:
        int x;
};

class Circle : public Shape {
    public:
        void draw();
        virtual void glow();
    private:
        int radius;
};
