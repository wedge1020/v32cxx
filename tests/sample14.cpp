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
