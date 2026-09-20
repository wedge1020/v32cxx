// Five real bugs found and fixed via a real user's Space Invaders program
// (see docs/DESIGN_NOTES.md for the full writeup):
//
//   1. A global (namespace-scope) object's method calls, including
//      explicitly namespace-qualified access from OUTSIDE its own
//      namespace (`ns::g_counter.next()`).
//   2. A `virtual ... const` method's vtable slot/cast staying const-
//      correct (Dog::sound/Cat::sound below are both `virtual ... const`).
//   3. An operator[] overload's result used for a further VIRTUAL method
//      call (`list[i]->sound()`).
//   4. Derived-to-base reference-parameter binding in overload resolution
//      (a Dog/Cat object passed where a `const Animal&` parameter is
//      declared), plus the explicit-cast argument lowering it needs.
//   5. Derived-to-base pointer ASSIGNMENT (`Animal *pet; pet = new Dog();`),
//      plus the explicit-cast lowering it needs.

namespace ns {
    class Counter {
    public:
        Counter(int start = 0) : mValue(start) {}
        int next() { mValue = mValue + 1; return mValue; }
        int mValue;
    };
    Counter g_counter;  // zero-arg construction, using the default
                         // parameter value -- matches the real pattern
                         // (a bare `Random g_rng;` at namespace scope,
                         // no constructor arguments) this bug was
                         // actually found from; namespace-scope direct-
                         // initialization WITH constructor arguments
                         // (`Counter g_counter(100);`) is a separate,
                         // real gap this test deliberately doesn't
                         // exercise -- see docs/DESIGN_NOTES.md.
}

class Animal {
public:
    virtual int sound() const { return 0; }
};

class Dog : public Animal {
public:
    virtual int sound() const { return 1; }
};

class Cat : public Animal {
public:
    virtual int sound() const { return 2; }
};

bool sameSound(const Animal &a, const Animal &b) {
    return a.sound() == b.sound();
}

class AnimalList {
public:
    AnimalList() : mCount(0) {}
    void push(Animal *a) {
        mItems[mCount] = a;
        mCount = mCount + 1;
    }
    Animal *operator[](int i) { return mItems[i]; }
    Animal *mItems[4];
    int mCount;
};

// Expected return value: 7 (a=1, b=2, dogSound=1, same=false->0, total=3).
int main() {
    int a = ns::g_counter.next();  // 1 (Counter's default param zeroes it)
    int b = ns::g_counter.next();  // 2

    Animal *pet;
    pet = new Dog();               // derived-to-base pointer assignment
    int dogSound = pet->sound();   // 1

    Cat cat;
    bool same = sameSound(*pet, cat);  // derived-to-base ref binding, both sides -- 1 != 2, so false

    AnimalList list;
    list.push(pet);
    list.push(&cat);
    int total = 0;
    for (int i = 0; i < list.mCount; i = i + 1) {
        total = total + list[i]->sound();  // operator[] result -> virtual dispatch
    }

    return a + b + dogSound + (same ? 1 : 0) + total;
}
