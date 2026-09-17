// Exercises sizeof -- both the type-taking form (sizeof(Type)) and the
// expression-taking form (sizeof expr, sizeof(expr)) -- confirming
// both reach the grammar correctly and, for the expression form
// specifically, that a call inside it still gets resolved (the
// systematic sweep this round's own DESIGN_NOTES.md entry describes).
//
// This project never computes a sizeof value itself -- the downstream
// C compiler does, real C's own rule -- so there is no expected
// NUMERIC value to check here; the point is confirming all three forms
// (type, bare expression, parenthesized expression, and an expression
// containing a call) parse and transpile correctly as literal
// `sizeof(...)`, verified by reading the generated C directly.

int makeValue() {
    return 5;
}

void main() {
    int a = sizeof(int);
    int b = sizeof(float);
    int x = 5;
    int c = sizeof x;
    int d = sizeof(x);
    int e = sizeof(makeValue());
}
