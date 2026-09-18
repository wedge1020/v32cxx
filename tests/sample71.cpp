// Exercises the ternary-to-if/else rewrite (--target=vircon32 only --
// the real Vircon32 C compiler doesn't support the ternary operator
// at all). Covers all three rewritable shapes (var_decl initializer,
// plain assignment to a bare identifier, return expression), chained/
// nested ternaries (fully unwound, not just the outermost level), and
// the documented scope boundary: a ternary nested inside a call
// argument is deliberately left untouched, even in vircon32 mode --
// see rewrite_ternary_stmt's own doc comment in lower.c for the full
// reasoning. In standard mode (--target=standard), none of this
// rewriting happens at all -- real standard C supports the ternary
// operator natively, so it's kept exactly as written.
//
// Expected: bigger = 10 (a var_decl-initializer ternary, 5 vs 10);
// assigned = 100 (a plain-assignment ternary, m=10 > 5); classified =
// 1 (classify's own chained ternary, x=5 is positive); viaCall = 6
// (add(x > y ? x : y, 1) -- the deliberately-untouched, nested-in-a-
// call-argument case -- still transpiles and still computes the
// right value, it just isn't valid standard Vircon32 C on its own).

int add(int a, int b) {
    return a + b;
}

int classify(int x) {
    return x < 0 ? -1 : (x == 0 ? 0 : 1);
}

void main() {
    int x = 5;
    int y = 10;

    int bigger = x > y ? x : y;

    int assigned;
    int m = 10;
    assigned = m > 5 ? 100 : 200;

    int classified = classify(x);

    int viaCall = add(x > y ? x : y, 1);
}
