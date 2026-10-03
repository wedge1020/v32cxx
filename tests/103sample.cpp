// tests/103sample.cpp
//
// References used as VALUES, self-checked on the real console.
//
// A C++ reference lowers to a C pointer. Member access through one
// (`r.x` -> `r->x`) has worked for a long time; everything else about
// using a reference did not:
//
//   - a reference parameter or local used as a whole value
//     (`a = a + 1`, `dst = src`, `int b = a`) was emitted as the bare
//     pointer -- pointer arithmetic and pointer assignment;
//   - a reference-returning OVERLOADED OPERATOR used as an lvalue
//     (`v[0] = 5`, `v[1] += a`) assigned to the returned pointer;
//   - a reference local bound to a reference return
//     (`Cell &r = grid.at(1)`) or to a plain variable (`int &r = n`)
//     was initialized with the value instead of the address.
//
// test_errors starts at -1 and must read 0 when main() finishes.

struct Cell { int x; int hp; };

class Grid {
public:
    int  nums[4];
    Cell cells[4];
    int  &operator[](int i) { return nums[i]; }
    Cell &at(int i)         { return cells[i]; }
    void set(int i, const int &v)   { nums[i] = v; }
    void put(int i, const Cell &c)  { cells[i] = c; }
};

void bump(int &a)                 { a = a + 1; a++; int b = a; a += b; }
void forward(int &a)              { bump(a); }
void copyCell(Cell &dst, const Cell &src) { dst = src; }
int  &larger(int &a, int &b)      { if (a > b) return a; return b; }

int test_errors = -1;

int main() {
    int errors = 0;

    // reference parameters as values
    int n = 1;
    bump(n);                       // 1 -> 2 -> 3, then 3 + 3
    if (n != 6) errors++;
    forward(n);                    // 6 -> 7 -> 8, then 8 + 8
    if (n != 16) errors++;

    // reference locals
    int &r = n;
    r = 4;
    if (n != 4) errors++;
    int m = r;
    m = m + 10;
    if (m != 14 || n != 4) errors++;

    // a reference bound to a reference return
    int &big = larger(n, m);
    big = 99;
    if (m != 99 || n != 4) errors++;

    // whole-struct copies through references
    Cell a; a.x = 1; a.hp = 2;
    Cell b; b.x = 0; b.hp = 0;
    copyCell(b, a);
    if (b.x != 1 || b.hp != 2) errors++;

    // a reference-returning operator as an lvalue
    Grid g;
    g[0] = 5;
    g[1] = g[0] + 1;
    g[1] += 10;
    g[2] = 0;
    g[2]++;
    if (g.nums[0] != 5 || g.nums[1] != 16 || g.nums[2] != 1) errors++;
    int v = 7;
    g.set(3, v);
    if (g[3] != 7) errors++;

    // a reference-returning method: member access, whole assignment,
    // and binding a reference local to it
    g.put(0, a);
    g.at(0).hp = 8;
    if (g.cells[0].x != 1 || g.cells[0].hp != 8) errors++;
    g.at(1) = a;
    if (g.cells[1].hp != 2) errors++;
    Cell &c = g.at(1);
    c.hp = 3;
    c = g.at(0);
    if (g.cells[1].hp != 8) errors++;

    test_errors = errors;
    return 0;
}
