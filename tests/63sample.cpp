// Exercises goto and a labeled statement together -- the highest-risk
// grammar addition of this round (see docs/DESIGN_NOTES.md for the
// full reasoning why: a bare identifier at the start of a statement is
// ambiguous with an ordinary expression-statement until the parser
// sees whether ':' follows it).
//
// Expected: result = 15 (1+2+3+4+5, the classic "goto as a loop"
// idiom). This project makes no attempt to validate that a goto lands
// somewhere sensible, nor to handle destructor invocation correctly
// across one (see AST_GOTO's own doc comment in ast.h for that
// explicit, known gap) -- this test deliberately stays within a single
// function's own flat scope with no destructible (class-typed) locals
// in play at all, avoiding that gap rather than exercising it.

int sumTo5() {
    int sum = 0;
    int i = 1;
loop:
    sum = sum + i;
    i = i + 1;
    if (i <= 5) {
        goto loop;
    }
    return sum;
}

void main() {
    int result = sumTo5();
}
