int test_errors = -1;
int buf[4] = {5, 6, 7, 8};
int target(void) { return 42; }
int via_addr(void) {
    int r = 0;
    asm {
        "mov R1, {&buf}"
        "iadd R1, 2"
        "mov R0, [R1]"
        "mov {r}, R0"
    }
    return r;
}
int via_deref(int *p) {
    int r = 0;
    asm {
        "mov R0, {*p}"
        "mov {r}, R0"
    }
    return r;
}
int main(void) {
    int loc = 9, got = 0, e = 0;
    if (via_addr() != 7) e |= 1;
    if (via_deref(&buf[1]) != 6) e |= 2;
    asm {
        "mov R1, {&loc}"
        "mov R0, [R1]"
        "mov {got}, R0"
    }
    if (got != 9) e |= 4;
    asm {
        "mov R1, {&target}"
        "mov {got}, R1"
    }
    if (got == 0) e |= 8;
    test_errors = e;
    return 0;
}
