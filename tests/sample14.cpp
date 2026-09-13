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

int doubleIt(int x) {
    return x * 2;
}
// A single declaration doing double duty as both the forward reference
// Shape::area/Circle::area need and doubleIt's real implementation --
// DELIBERATELY not split into a separate `int doubleIt(int x);`
// declaration followed later by this definition, the way a prototype-
// then-implementation free function ordinarily would be in real C++.
// That split would register TWO separate entries for the same name in
// this project's free-function registry (nothing pairs a free
// function's prototype to its own later definition the way
// attach_out_of_line does for methods) -- resolve_call would then see
// two identically-shaped candidates for every call to doubleIt() and
// report it as ambiguous, a real, newly-discovered gap this project has
// rather than a deliberate scope choice. Tracked in docs/DESIGN_NOTES.md
// as a genuine limitation; worked around here rather than fixed, since
// fixing it properly means giving free functions the same prototype-to-
// definition matching methods already have, which is a bigger change
// than this test file's own needs justify on its own. This project's
// own multi-pass design doesn't care about declare-before-use ordering
// at all, so moving the full definition here (instead of split further
// down, closer to the out-of-line method definitions) changes nothing
// about what actually gets resolved.

int Shape::area() {
    return doubleIt(size);   // free-function call
}

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
