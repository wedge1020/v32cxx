// Exercises do-while -- the body runs at least once even when the
// condition is false from the very start, unlike an ordinary `while`
// (real C++'s own defining distinction between the two). Also exercises
// `break` inside a do-while, confirming the SAME loop-depth/destructor-
// boundary machinery ordinary `while`/`for` already use works
// correctly here too -- by design this project reuses AST_WHILE for
// both (see AST_WHILE's own doc comment in ast.h), so sema.c and
// lower.c needed zero changes at all for this; this test is the real
// confirmation of that claim, not just the reasoning behind it.
//
// Expected: a = 1 (runOnce's own body executes exactly once despite
// its condition being `false` from the start); b = 15 (sumUpTo5:
// 1+2+3+4+5); c = 12 (findFirstOver10: 0, then 3, 6, 9, 12 -- breaks
// the moment i exceeds 10, at i=12).

int runOnce() {
    int count = 0;
    do {
        count = count + 1;
    } while (false);
    return count;
}

int sumUpTo5() {
    int sum = 0;
    int i = 1;
    do {
        sum = sum + i;
        i = i + 1;
    } while (i <= 5);
    return sum;
}

int findFirstOver10() {
    int i = 0;
    do {
        i = i + 3;
        if (i > 10) {
            break;
        }
    } while (i < 100);
    return i;
}

void main() {
    int a = runOnce();
    int b = sumUpTo5();
    int c = findFirstOver10();
}
