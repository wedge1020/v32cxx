// Exercises the standard-C function-pointer declarator form --
// ReturnType (*name)(ParamTypes); -- confirming it's accepted as valid
// C++-side input and, per Vircon32's own quirky declarator syntax
// (docs/VIRCON32_QUIRKS.md), always emitted in Vircon32's OWN required
// form regardless (ReturnType(ParamTypes)* name;). Also confirms a
// function pointer can be ASSIGNED a function's own name and CALLED
// through it, both already working via this project's existing,
// generic expression grammar with no new work needed for either --
// and exercises the `(void)` "no parameters" spelling specifically.
//
// Expected: result = 8 (add(5, 3), called indirectly through fp);
// zero = 0 (getZero(), called indirectly through a no-argument
// function pointer declared with the `(void)` spelling).

int add(int a, int b) {
    return a + b;
}

int getZero(void) {
    return 0;
}

void main() {
    int (*fp)(int, int);
    fp = add;
    int result = fp(5, 3);

    int (*zfp)(void);
    zfp = getZero;
    int zero = zfp();
}
