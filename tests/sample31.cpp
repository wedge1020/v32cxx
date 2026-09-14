// Deliberately invalid: `continue` used outside any loop at all. Expect
// a clean semantic error from sema.c's new loop-depth check (real
// C++/C both reject this too) and a non-zero exit code -- not a crash,
// and not a silent misparse.

void doSomething() {
    continue;
}
