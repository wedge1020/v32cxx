// Deliberately invalid: Base has a constructor, but it's not callable
// with zero arguments (only Base(int)), and Derived doesn't explicitly
// delegate to it at all -- exactly the case real C++ rejects with "no
// default constructor exists for base class". Should produce exactly
// one semantic error, from check_implicit_base_construction, not a
// silently uninitialized base subobject the way this would have
// compiled before this check existed.

class Base {
    public:
        Base(int v);
    private:
        int v;
};

Base::Base(int v) {
    this->v = v;
}

class Derived : public Base {
    public:
        Derived(int x);
    private:
        int x;
};

Derived::Derived(int x) : x(x) {
}

void main() {
}
