// *****************************************************************************
//  tests/124sample.cpp — a call whose result is written through runs ONCE
//
//  The Vircon32 C compiler (v26.04.24) evaluates a call more than once when
//  a statement writes through the pointer it returns -- `*get() = 5` and
//  `get()->m = 7` call get() twice, `get()->m += 1` three times -- and
//  `(*get()).n` calls it twice even when only reading (VIRCON32_QUIRKS.md
//  #27). Every reference-returning function or operator used as a target
//  has exactly that shape in the generated C, so v32c++ stores the pointer
//  first (writes) and prints `(get())->n` (reads). Each check below counts
//  the calls.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;
int g_calls = 0;

struct Cell
{
    int n;
    int m;
};

Cell g_cell;
int  g_value = 0;

Cell &cell()     { g_calls++; return g_cell; }
int  &value()    { g_calls++; return g_value; }
Cell *cell_ptr() { g_calls++; return &g_cell; }

class Grid
{
    public:
        int cells[4];
        int lookups;
        Grid() { lookups = 0; for (int i = 0; i < 4; i++) cells[i] = 0; }
        int &operator[](int i) { lookups++; return cells[i]; }
        int &at(int i) { lookups++; return cells[i]; }
};

class Iter
{
    public:
        int n;
        int steps;
        Iter() { n = 0; steps = 0; }
        Iter &operator++() { steps++; n++; return *this; }
};

int main()
{
    int e = 0;

    g_calls = 0; value() = 5;              if (g_calls != 1 || g_value != 5) e++;
    g_calls = 0; value() += 2;             if (g_calls != 1 || g_value != 7) e++;
    g_calls = 0; value()++;                if (g_calls != 1 || g_value != 8) e++;
    g_calls = 0; ++value();                if (g_calls != 1 || g_value != 9) e++;
    g_calls = 0; cell().n = 3;             if (g_calls != 1 || g_cell.n != 3) e++;
    g_calls = 0; cell().m += 4;            if (g_calls != 1 || g_cell.m != 4) e++;
    g_calls = 0; cell_ptr()->n = 6;        if (g_calls != 1 || g_cell.n != 6) e++;
    g_calls = 0; cell_ptr()->m++;          if (g_calls != 1 || g_cell.m != 5) e++;
    g_calls = 0; int r = cell().n;         if (g_calls != 1 || r != 6) e++;
    g_calls = 0; r = (*cell_ptr()).m;      if (g_calls != 1 || r != 5) e++;

    Grid g;
    g[1] = 10;
    g[2] += 5;
    g.at(3) = 7;
    g[3]++;
    if (g.lookups != 4) e++;
    if (g.cells[1] != 10 || g.cells[2] != 5 || g.cells[3] != 8) e++;

    Iter it;
    int seen = (++it).n;
    if (it.steps != 1 || seen != 1) e++;
    (++it).n = 10;
    if (it.steps != 2 || it.n != 10) e++;

    test_errors = e;
    return 0;
}
