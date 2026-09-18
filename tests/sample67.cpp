// Exercises multi-dimensional arrays, in both accepted declarator
// styles -- standard-C (`int grid[3][3];`) and Vircon32-native
// (`int [2][2] grid2;`) -- confirming both produce identically-nested
// AST_ARRAY_TYPE structures and Vircon32-required output with the
// dimensions in the correct order (`int [3][3] grid;`, outermost
// dimension first, matching real C's own multi-dimensional array
// semantics -- a real bug caught and fixed before shipping this: see
// print_type's own AST_ARRAY_TYPE case in codegen.c for why the naive
// single-dimension approach doesn't generalize to nesting on its own).
// Chained subscripting (`grid[i][j]`) needed no new grammar at all --
// postfix_expr's own subscript rule was already left-recursive.
//
// Expected: sum = 36 (filling a 3x3 grid with grid[i][j] = i*3+j,
// i.e. 0 through 8, then summing every cell: 0+1+...+8 = 36);
// sum2 = 10 (a 2x2 grid holding 1, 2, 3, 4).

void main() {
    int grid[3][3];
    int sum = 0;
    for (int i = 0; i < 3; i = i + 1) {
        for (int j = 0; j < 3; j = j + 1) {
            grid[i][j] = i * 3 + j;
        }
    }
    for (int i = 0; i < 3; i = i + 1) {
        for (int j = 0; j < 3; j = j + 1) {
            sum = sum + grid[i][j];
        }
    }

    int [2][2] grid2;
    grid2[0][0] = 1;
    grid2[0][1] = 2;
    grid2[1][0] = 3;
    grid2[1][1] = 4;
    int sum2 = grid2[0][0] + grid2[0][1] + grid2[1][0] + grid2[1][1];
}
