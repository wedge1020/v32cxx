// Exercises the ternary-to-if/else rewrite (--target=vircon32 only --
// the real Vircon32 C compiler doesn't support the ternary operator
// at all). Covers all three DIRECT, no-temp rewritable shapes
// (var_decl initializer, plain assignment to a bare identifier,
// return expression), chained/nested ternaries (fully unwound, not
// just the outermost level), AND a ternary nested inside a call
// argument (add(x > y ? x : y, 1)) -- this last case used to be a
// real, previously-undiscovered miscompile: the real Vircon32 C
// lexer doesn't even recognize '?' as a token, so the transpiled
// output failed to compile at all ("character '?' is not a valid
// identifier start"), not merely a missed style optimization. Fixed
// by hoisting any ternary the three direct shapes can't reach into
// its own preceding temp variable, set via an ordinary if/else -- see
// hoist_ternaries_in_expr's own doc comment in lower.c for the full
// reasoning and its two remaining, still-documented boundaries
// (a for-loop's own init/cond/incr clauses; a brace-less single-
// statement slot). In standard mode (--target=standard), none of this
// rewriting happens at all -- real standard C supports the ternary
// operator natively, so it's kept exactly as written.
//
// Expected: bigger = 10 (a var_decl-initializer ternary, 5 vs 10);
// assigned = 100 (a plain-assignment ternary, m=10 > 5); classified =
// 1 (classify's own chained ternary, x=5 is positive); viaCall = 11
// (add(x > y ? x : y, 1) -- x=5, y=10, so the ternary picks y=10,
// add(10, 1) = 11 -- the nested-in-a-call-argument case, now actually
// hoisted and confirmed to both transpile AND compile in Vircon32
// mode, not just compute the right value in standard mode).

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
