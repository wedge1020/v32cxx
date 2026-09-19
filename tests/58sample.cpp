// Exercises the ternary/conditional operator -- cond ? true : false --
// including chaining (right-associative, matching real C++: `a ? b : c
// ? d : e` parses as `a ? b : (c ? d : e)`, not `(a ? b : c) ? d : e`)
// and confirming it sits at the correct precedence relative to
// assignment (assignment binds LOOSER than ternary, so
// `assigned = m > 5 ? 100 : 200` assigns the WHOLE ternary result to
// assigned, not `(assigned = (m > 5)) ? 100 : 200`).
//
// Expected: m = 10 (max of 5, 10); negResult = -1, zeroResult = 0,
// posResult = 1 (classify's own chained ternary picking the right
// branch each time); assigned = 100 (m=10 > 5, so the true branch).

int maxOf(int a, int b) {
    return a > b ? a : b;
}

int classify(int x) {
    return x < 0 ? -1 : (x == 0 ? 0 : 1);
}

void main() {
    int m = maxOf(5, 10);
    int negResult = classify(-5);
    int zeroResult = classify(0);
    int posResult = classify(5);
    int assigned;
    assigned = m > 5 ? 100 : 200;
}
