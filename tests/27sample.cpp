// Exercises destructor invocation at scope exit (lower.c phase 9) --
// none of this project's existing tests exercise a stack-allocated
// class object going out of scope at all (sample22/23's Player has no
// destructor; sample25's Logger is only ever used as a pointer, via
// new/delete, never as a stack value). This is the first test with a
// class that's both destructor-having AND ever stack-allocated.
//
// compute() specifically exercises the harder cases: an early `return`
// from inside a nested block (with locals live in BOTH the inner scope
// and the enclosing one, needing destruction innermost-first), a
// non-void `return expr;` (needing lower.c's temporary-variable
// rewrite, since `expr` must be evaluated before any destructor runs),
// and a second early return after the first scope has already closed
// (confirming already-exited locals correctly drop out of the
// destruction set). main() exercises the simpler, single fall-through
// case.

class Logger {
    public:
        Logger();
        ~Logger();
    private:
        int id;
};

Logger::Logger() {
    this->id = 0;
}

Logger::~Logger() {
    this->id = -1;
}

int compute(int flag) {
    Logger a;
    if (flag) {
        Logger b;
        return flag;
    }
    Logger c;
    return 0;
}

void main() {
    Logger d;
    compute(1);
}
