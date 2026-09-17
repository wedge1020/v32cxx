// Exercises a global (file-scope) variable -- a real, previously-
// existing bug, not a new feature: the grammar already accepted this
// syntax, but codegen_run had no function that actually EMITTED one,
// so the declaration was silently dropped -- any function referencing
// it produced generated C that referenced an undeclared identifier, a
// real compile failure downstream. Also exercises sema.c's own
// check_globals -- a global's own initializer expression now gets
// walked and its own calls resolved, the same as a local variable's
// initializer already was.
//
// Expected: main() calls increment() twice, then decrement() once,
// leaving counter at 1 (0 + 1 + 1 - 1). getCounter() returning it lets
// this be checked by reading the generated C directly (counter's own
// value isn't otherwise observable from outside main()).

int counter = 0;

void increment() {
    counter = counter + 1;
}

void decrement() {
    counter = counter - 1;
}

int getCounter() {
    return counter;
}

void main() {
    increment();
    increment();
    decrement();
    int result = getCounter();
}
