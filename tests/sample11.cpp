// Exercises call-site overload resolution: method calls and free-function
// calls whose argument count/types pick a specific overload among
// several, and a genuine no-match case (an arity that matches none of
// the existing overloads).
//
// Expected: the "call resolutions:" section of the sema dump lists FIVE
// resolved calls (three in useCalculator, two in useFreeOverloads), each
// with a mangled target matching the argument count/types used at that
// call site, and exactly ONE semantic error, from BadCaller::oops's
// 3-argument call, which matches none of Calculator::add's overloads
// (1 or 2 arguments only).

class Calculator {
    public:
        Calculator(int start);
        int add(int x);
        int add(int x, int y);
        float add(float x);
    private:
        int total;
};

void useCalculator(Calculator c) {
    int a = c.add(5);        // resolves to add(int)
    int b = c.add(5, 10);     // resolves to add(int, int)
    float f = c.add(2.5);      // resolves to add(float)
}

int combine(int x, int y);
float combine(float x);

void useFreeOverloads() {
    int r1 = combine(1, 2);   // resolves to combine(int, int)
    float r2 = combine(1.5);   // resolves to combine(float)
}

class BadCaller {
    public:
        void oops(Calculator c);
};

void BadCaller::oops(Calculator c) {
    c.add(1, 2, 3);   // ERROR: no Calculator::add overload takes 3 arguments
}
