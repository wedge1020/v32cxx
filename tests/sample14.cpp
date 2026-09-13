// Exercises vtable dispatch codegen (lowering phase 3): a virtual call
// becomes obj->vtable->FIELD(obj, args...); a non-virtual method call
// becomes a direct call to the mangled function name; a free-function
// call becomes a direct call to its own mangled name.
//
// The important thing to check: Circle::describeTwice() calls area()
// (virtual, overridden in Circle) -- the vtable FIELD NAME used at that
// call site must still be Shape's mangled name (Shape__area__void), NOT
// Circle's, even though Circle::area is the actual implementation that
// will end up stored in that field at runtime. The field name identifies
// the SLOT (fixed at whichever class first declared it virtual); only
// the function pointer VALUE stored there varies per class, which is a
// separate, later concern this phase doesn't handle.
//
// Also check: describeTwice() calling the INHERITED (non-overridden,
// non-virtual) describe() becomes a direct call to Shape__describe__void
// -- confirming non-virtual dispatch still works correctly from a
// derived class.

class Shape {
    public:
        virtual int area();
        int describe();
    protected:
        int size;
};

int Shape::describe() {
    int a = area();      // virtual call, dispatched through the vtable
    return a;
}

int doubleIt(int x);

int Shape::area() {
    return doubleIt(size);   // free-function call
}

int doubleIt(int x) {
    return x * 2;
}
// Back to the natural, ordinary C++ pattern -- declare, then define
// separately later -- now that sema.c's register_free_function dedupes
// an exact name+signature match against an existing registry entry
// instead of registering a second, identically-shaped candidate. This
// used to need a workaround here (a single combined declaration) to
// avoid resolve_call misreporting every call to doubleIt() as
// ambiguous; see docs/DESIGN_NOTES.md for the full story. Restoring
// this pattern is itself a real test of that fix, in addition to
// tests/sample21.cpp's more isolated one.

class Circle : public Shape {
    public:
        int area();          // override -- doesn't repeat 'virtual'
        int describeTwice();
};

int Circle::describeTwice() {
    int a = area();          // virtual call FROM an overriding class --
                              // still uses Shape's field name (see above)
    int b = describe();       // non-virtual call to an INHERITED method
    return a + b;
}

int Circle::area() {
    return doubleIt(size * 2);
}

void main() {
    // Deliberately empty -- Vircon32 requires an actual `main` to exist
    // (a whole-cartridge entry point, no OS to be a library function
    // for), which this file needed to compile standalone at all, but
    // this test is specifically about call-site codegen, not about
    // constructing objects. Vtable STATIC INSTANCES don't exist yet
    // (see docs/DESIGN_NOTES.md) -- actually instantiating a Shape or
    // Circle here and calling a virtual method on it would compile but
    // behave incorrectly (an uninitialized vtable pointer), which would
    // be a misleading thing for a test to appear to demonstrate.
}
