// Exercises Vircon32's own native function-pointer declarator form --
// ReturnType(ParamTypes)* name; -- as an ALTERNATE valid C++-side
// input spelling, confirming it produces the exact same behavior (and
// identical generated output, since the AST carries no memory of
// which spelling was used) as the standard-C form in sample64.cpp.
//
// Expected: result = 15 (multiply(5, 3), called indirectly through fp).

int multiply(int a, int b) {
    return a * b;
}

void main() {
    int(int, int)* fp;
    fp = multiply;
    int result = fp(5, 3);
}
