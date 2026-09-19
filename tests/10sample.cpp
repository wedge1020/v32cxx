// Exercises access-control ENFORCEMENT (not just tracking): legal private
// access from within the owning class, illegal private access from a
// derived class (even via implicit this->, not just explicit obj.member),
// legal protected access from a derived class, illegal protected/private
// access from an unrelated class, and legal public access from anywhere
// (including a free function with no calling context at all).
//
// Expected: exactly four semantic errors (touchPrivate's implicit access,
// Unrelated::poke's two explicit accesses, and freeFunctionPoke's one),
// and no errors for anything else -- touchProtected, getValue via "b",
// and the constructors should all be silent.

class Base {
    public:
        Base(int v);
        int getValue();
    protected:
        int protectedValue;
    private:
        int secretValue;
};

class Derived : public Base {
    public:
        Derived(int v);
        void touchProtected();
        void touchPrivate();
};

void Derived::touchProtected() {
    protectedValue = 1;   // OK: implicit this-> to an inherited protected member
}

void Derived::touchPrivate() {
    secretValue = 1;   // ERROR: Base's private member, not accessible even from Derived
}

class Unrelated {
    public:
        void poke(Base b);
};

void Unrelated::poke(Base b) {
    b.protectedValue = 1;   // ERROR: protected, Unrelated is not derived from Base
    b.secretValue = 1;       // ERROR: private, never accessible from outside Base
    int ok = b.getValue();   // OK: public
}

void freeFunctionPoke(Base b) {
    b.secretValue = 1;   // ERROR: private, and no calling-context class at all
}
