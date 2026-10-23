int test_errors = -1;
int abs2(int x) { return x < 0 ? -x : x; }
struct S { int abs; };
int main(void) {
    struct S s;
    s.abs = 4;
    test_errors = (abs2(-5) == 5 && s.abs == 4) ? 0 : 1;
    return 0;
}
