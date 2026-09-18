// Exercises arrays of function pointers, in BOTH accepted declarator
// styles -- standard-C (`ReturnType (*name[N])(ParamTypes);`) and
// Vircon32-native (`ReturnType(ParamTypes)* [N] name;`). The Vircon32
// array-of-function-pointers spelling specifically is this project's
// own extrapolation from its two individually-confirmed patterns (the
// plain Vircon32 function-pointer form, and the plain Vircon32 array
// form), NOT yet confirmed end to end against the real compiler -- see
// AST_FUNC_PTR_TYPE's own doc comment in ast.h and that production's
// own comment in parser.y.
//
// Expected: r0 = 8 (ops[0] is add, called through the standard-C-
// declared array), r1 = 2 (ops[1] is subtract); r2 = 15 (ops2[0] is
// multiply, called through the Vircon32-declared array).

int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int multiply(int a, int b) {
    return a * b;
}

void main() {
    int (*ops[2])(int, int);
    ops[0] = add;
    ops[1] = subtract;
    int r0 = ops[0](5, 3);
    int r1 = ops[1](5, 3);

    int(int, int)* [1] ops2;
    ops2[0] = multiply;
    int r2 = ops2[0](5, 3);
}
