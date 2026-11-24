// *****************************************************************************
//  tests/180sample.cpp -- `goto` runs the destructors of what it leaves
//
//  A goto that leaves scopes holding class objects (or arrays of them) already
//  built destroys them, last built first, before it jumps -- exactly what
//  break/continue/return already did. What is live at the label is not
//  touched: a forward goto, a goto to a label in the same block, and a
//  backward goto (a loop made with goto: the object declared after the label
//  is destroyed at the goto and built again) all follow C++.
//
//  A constructor logs its id, a destructor the negated id. test_errors must
//  end at 0. (tests/180bad.cpp holds the jumps C++ forbids.)
// *****************************************************************************

int test_errors = -1;
int g_log[64];
int g_n = 0;
int g_live = 0;

class D {
public:
    int id;
    D() { id = 9; g_log[g_n] = 9; g_n = g_n + 1; g_live = g_live + 1; }
    D(int i) { id = i; g_log[g_n] = i; g_n = g_n + 1; g_live = g_live + 1; }
    ~D() { g_log[g_n] = 0 - id; g_n = g_n + 1; g_live = g_live - 1; }
};

int e = 0;

// compares the log with the n values in `want` (and that no object is left alive), then clears it
void check(const int *want, int n)
{
    if (g_n != n) e++;
    for (int i = 0; i < n && i < g_n; i++)
        if (g_log[i] != want[i]) e++;
    if (g_live != 0) e++;
    g_n = 0;
}

// forward goto out of two nested blocks: inner first, then the outer one
void out_of_nested(int go)
{
    D a(1);
    {
        D b(2);
        {
            D c(3);
            if (go) goto done;
        }
    }
done:
    ;
}

// the label is in the same block, after the goto: nothing built yet to destroy
void same_block(void)
{
    D a(1);
    goto skip;
skip:
    ;
}

// backward goto: the loop body's object is destroyed and built again
int count_loop(void)
{
    int n = 0;
again:
    D d(10 + n);
    n++;
    if (n < 3) goto again;
    return n;
}

// goto out of a loop that holds an array and a single object
void out_of_loop(void)
{
    D outer(1);
    for (int i = 0; i < 5; i++) {
        D arr[2] = { D(20), D(21) };
        D inner(3);
        if (i == 1) goto leave;
    }
leave:
    ;
}

// jumping INTO a block is fine while no declaration is bypassed
void into_block(int go)
{
    D a(1);
    if (go) goto inside;
    {
        D skipped(2);   // not reached when jumping: a goto must not land after this
    }
    {
inside:
        D late(3);
    }
}

// two gotos to one label, from different depths
int two_sources(int which)
{
    D a(1);
    if (which == 0) goto fin;
    {
        D b(2);
        if (which == 1) goto fin;
        D c(3);
        if (which == 2) goto fin;
    }
fin:
    return which;
}

// an unbraced `if (x) goto` still gets its destructors
void bare_if(int go)
{
    D a(1);
    {
        D b(2);
        if (go) goto end;
    }
end:
    ;
}

int main(void)
{
    {   int w[] = { 1, 2, 3, -3, -2, -1 };
        out_of_nested(1); check(w, 6);
        out_of_nested(0); check(w, 6);   // no jump: c and b go at their own braces, same order
    }
    {   int w[] = { 1, -1 };
        same_block(); check(w, 2);
    }
    {   int w[] = { 10, -10, 11, -11, 12, -12 };
        if (count_loop() != 3) e++;
        check(w, 6);
    }
    {   // each pass: arr, inner; the jump (i == 1) destroys inner, then arr; outer goes last
        int w[] = { 1, 20, 21, 3, -3, -21, -20, 20, 21, 3, -3, -21, -20, -1 };
        out_of_loop(); check(w, 14);
    }
    {   int w[] = { 1, 3, -3, -1 };
        into_block(1); check(w, 4);
    }
    {   int w[] = { 1, 2, -2, 3, -3, -1 };
        into_block(0); check(w, 6);
    }
    {   int w[] = { 1, -1 };
        if (two_sources(0) != 0) e++;
        check(w, 2);
    }
    {   int w[] = { 1, 2, -2, -1 };
        if (two_sources(1) != 1) e++;
        check(w, 4);
    }
    {   int w[] = { 1, 2, 3, -3, -2, -1 };
        if (two_sources(2) != 2) e++;
        check(w, 6);
    }
    {   int w[] = { 1, 2, -2, -1 };
        bare_if(1); check(w, 4);
    }
    test_errors = e;
    return 0;
}
