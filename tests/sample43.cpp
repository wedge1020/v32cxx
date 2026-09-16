// Exercises IMPLICIT base-class construction -- a derived constructor
// that does NOT explicitly delegate to its base (no ": Base(args)" at
// all) still gets a call to the base's own zero-argument constructor
// inserted automatically, matching real C++'s own rule. Base's own
// `tag` is private, same "no other legal way to set it" motivation as
// explicit delegation's own tests -- proving the implicit call actually
// ran, not just that the program compiles.
//
// Expected: getTag() returns 42 (Base's own zero-arg constructor sets
// it), even though Derived's own constructor never mentions Base at all.

class Base {
    public:
        Base();
        int getTag();
    private:
        int tag;
};

Base::Base() {
    this->tag = 42;
}

int Base::getTag() {
    return tag;
}

class Derived : public Base {
    public:
        Derived(int x);
        int getX();
    private:
        int x;
};

Derived::Derived(int x) : x(x) {
}

int Derived::getX() {
    return x;
}

void main() {
    Derived *d = new Derived(7);
    int tag = d->getTag();
    int x = d->getX();
    delete d;
}
