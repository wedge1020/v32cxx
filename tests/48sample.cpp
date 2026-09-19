// Exercises a switch NESTED inside a loop, with `continue` inside the
// switch -- confirms continue correctly bypasses the switch entirely
// and targets the ENCLOSING LOOP (real C's own rule: continue never
// targets a switch, only the nearest loop), while `break` inside that
// same switch correctly targets just the switch, not the loop -- the
// two behaving differently from each other is the entire point of
// this test.
//
// Trace by hand: i=0 -> default, sum=0; i=1 -> default, sum=1;
// i=2 -> case 2, i becomes 3, continue (skips the loop's own
// "i = i + 1" entirely, jumping straight back to the while condition);
// i=3 -> default, sum=4; i=4 -> default, sum=8; i=5 -> loop condition
// false, exit. Expected: sum = 0 + 1 + 3 + 4 = 8 (2 is skipped).

int sumSkipping2() {
    int sum = 0;
    int i = 0;
    while (i < 5) {
        switch (i) {
            case 2:
                i = i + 1;
                continue;
            default:
                sum = sum + i;
                break;
        }
        i = i + 1;
    }
    return sum;
}

void main() {
    int result = sumSkipping2();
}
