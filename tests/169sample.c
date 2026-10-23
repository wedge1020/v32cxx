int test_errors = -1;
int calls = 0;
int cycles = 0;
int get_cycle_counter(void) { return ++cycles; }
void yield(void) { calls++; cycles = 0; }
__attribute__((no_yield)) int quiet(int x) { int i = 0; while (i < 50) i++; return x + i; }
int busy(int n) {
    int s = 0, i;
    for (i = 0; i < n; i++) s += i;
    return s;
}
int main(void) {
    int e = 0, before;
    before = calls;
    quiet(1);
    if (calls != before) e |= 1;
    if (busy(100) != 4950) e |= 2;
    if (calls == before) e |= 4;
    test_errors = e;
    return 0;
}
