// Exercises switch/case/default with real C fall-through semantics --
// case 1 deliberately has NO break, so it falls through into case 2's
// own code too (real C behavior, not special-cased anywhere in this
// project -- see AST_SWITCH's own doc comment in ast.h for why this
// project needs no explicit fall-through logic at all: the generated
// C is just a literal switch, and the C compiler downstream provides
// the fall-through itself).
//
// Expected: classify(1) returns 30 (falls through case 1 into case 2,
// accumulating 10 then 20); classify(2) returns 20 (just case 2);
// classify(3) returns 5 (default); classify(4) returns 5 too (default,
// since there's no case 4 at all).

int classify(int x) {
    int result = 0;
    switch (x) {
        case 1:
            result = result + 10;
        case 2:
            result = result + 20;
            break;
        default:
            result = 5;
            break;
    }
    return result;
}

void main() {
    int a = classify(1);
    int b = classify(2);
    int c = classify(3);
    int d = classify(4);
}
