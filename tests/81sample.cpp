// Default parameter values -- exercises all four shapes this project's
// lowering has to fill missing trailing arguments back in for, since
// Vircon32 C (like plain C) has no default-argument mechanism of its own
// and every call in the generated C must supply every argument explicitly:
//
//   1. An ordinary free function call, some arguments omitted.
//   2. A constructor invoked with fewer arguments than it declares
//      (direct-initialization: `Random r2(99);`).
//   3. A constructor invoked with NO arguments at all, where the
//      constructor still has real (all-defaulted) parameters -- the
//      "ClassName var;" implicit-default-construction case, and its
//      per-element stack-array counterpart.
//   4. A derived class whose own constructor doesn't explicitly delegate
//      to its base at all -- the base's own all-defaulted constructor is
//      still the one implicitly called, with its defaults filled in.

class Random {
public:
    Random(int seed = 1234) : mState(seed) {}
    int next() {
        mState = mState * 1103515245 + 12345;
        return mState;
    }
    int mState;
};

int addThree(int a, int b = 10, int c = 20) {
    return a + b + c;
}

class Base {
public:
    Base(int x = 7) : mX(x) {}
    int mX;
};

class Derived : public Base {
public:
    Derived() {}
    int getX() { return mX; }
};

class Widget {
public:
    Widget(int v = 42) : mV(v) {}
    int mV;
};

int main() {
    Random r1;              // shape 3: zero-arg call to an all-defaulted ctor
    Random r2(99);           // shape 2: direct-init, fewer args than declared
    int x = addThree(1);              // shape 1: two args defaulted
    int y = addThree(1, 2);           // shape 1: one arg defaulted
    int z = addThree(1, 2, 3);        // shape 1: no args defaulted (baseline)

    Derived d;                // shape 4: implicit base call, base ctor defaulted
    Widget widgets[3];        // shape 3 (array form): per-element ctor call

    return r1.next() + r2.next() + x + y + z + d.getX() + widgets[0].mV;
}
