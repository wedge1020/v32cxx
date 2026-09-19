// Exercises a plain, C-style union -- overlapping storage for
// int/float, matching real C's own union semantics (Vircon32 C already
// has this natively, so this is passed through as literal, unmodified
// union syntax -- no lowering happens for it at all, see
// AST_UNION_DECL's own doc comment in ast.h).
//
// No numeric expected value to check here -- writing through one
// member and reading through another is exactly the kind of type-
// punning a union's overlapping storage is FOR, and the result depends
// on the platform's own representation, not anything this project
// computes. The point is confirming the declaration and member access
// both parse and transpile correctly, verified by reading the
// generated C directly (a real `union Value { ... };`, not a struct).

union Value {
    int i;
    float f;
};

void main() {
    Value v;
    v.i = 42;
}
