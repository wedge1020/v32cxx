// Exercises the "base has NO constructor at all" case specifically --
// real C++'s own implicitly-default-constructible rule: a class with
// no user-declared constructor of its own needs nothing called for it
// at all, so a derived class that doesn't mention it in any
// member-initializer list is still perfectly valid, not an error.
// Confirms this project's own check_implicit_base_construction
// correctly distinguishes "no constructor at all" (fine) from "has a
// constructor, but none zero-arg" (sample45.cpp's own error case).

class Base {
    public:
        int value;
};

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
    Derived *d = new Derived(9);
    int x = d->getX();
    delete d;
}
