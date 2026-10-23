int test_errors = -1;
const int squares[] __attribute__((rom)) = { 0, 1, 4, 9, 16, 25, -3 };
__attribute__((rom)) const int wide[10] = { 7, 'A' };
int sum(const int *p, int n) { int i, s = 0; for (i = 0; i < n; i++) s += p[i]; return s; }
int main(void) {
    int e = 0;
    if (squares[3] != 9) e |= 1;
    if (sum(squares, 6) != 55) e |= 2;
    if (sizeof(squares) != 7) e |= 4;
    if (wide[1] != 65 || wide[9] != 0 || wide[0] != 7) e |= 8;
    if (*(squares + 6) != -3) e |= 16;
    test_errors = e;
    return 0;
}
