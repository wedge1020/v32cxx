// *****************************************************************************
//  tests/185sample.cpp -- `--alloc=gc`: a pointer one past the end keeps ITS block alive
//
//  In C++ a pointer one past the end of an array is valid (`p + n`, the end() of a
//  buffer) and a program may keep nothing else. Blocks of the gc heap lie one after
//  the other, so such a pointer used to point at the header of the NEXT block: that
//  one was kept alive and the one the pointer belongs to was freed. Every block now
//  has one trailing unit (as in Boehm's collector), which lies inside the block:
//  p + n of `new int[n]`, and of `new D[n]` (with a count header), still protects
//  the array.
//
//  Built with --alloc=gc --heap-words=16384 by `make test`. The array is made in a
//  helper that returns, the heap is churned afterwards to reuse any block a bad
//  collection freed, and the neighbour (allocated right after the array, so it is
//  the block the old code kept) must be gone: gc_used() is back to one array's
//  worth. As in 182sample a stale copy of the neighbour's pointer on the stack
//  could in principle keep it (scrub() clears the dead stack first). That the array
//  survives does not depend on this: only the end pointer is kept. test_errors must
//  end at 0.
// *****************************************************************************
#include <v32/memory.hpp>

int test_errors = -1;
int e = 0;

class D {
public:
    int id;
    D() { id = 5; }
    ~D() { id = 0; }
};

int *g_end = 0;       // one past the end of an int array: the only reference to it
D *g_dend = 0;        // the same for an array of objects with a count header
int g_gap = 0;        // how far the neighbour's first word is from the end pointer

void scrub()
{
    volatile int pad[4096];
    for (int i = 0; i < 4096; i++) pad[i] = 0;
}

void collect_now()
{
    scrub();
    v32::gc_collect();
}

// reuses any block a bad collection freed
void churn()
{
    for (int i = 0; i < 300; i++) {
        int *t = new int[8];
        t[0] = -1;
        t[7] = -1;
    }
}

// two arrays, one after the other; only a one-past-end pointer of the first is kept
void make_ints()
{
    int *a = new int[8];
    int *b = new int[8];
    for (int i = 0; i < 8; i++) {
        a[i] = 100 + i;
        b[i] = 200 + i;
    }
    g_end = a + 8;
    g_gap = (int)(b - g_end);
}

void make_objects()
{
    D *a = new D[3];
    int *b = new int[8];
    b[0] = 1;
    g_dend = a + 3;
}

int main(void)
{
    collect_now();
    int base = v32::gc_used();
    int one;
    {   int *probe = new int[8];
        one = v32::gc_used() - base;       // what one int[8] takes, trailing unit included
        probe[0] = 0;
        probe = 0;
    }
    collect_now();
    if (v32::gc_used() != base) e++;

    make_ints();
    if (v32::gc_used() != base + 2 * one) e++;
    if (g_gap <= 0 || g_gap > 16) e++;      // the neighbour does come right after the end pointer
    collect_now();
    churn();
    // the array is alive: its contents are intact through the end pointer alone
    for (int i = 0; i < 8; i++)
        if (g_end[i - 8] != 100 + i) e++;
    // the neighbour is gone
    collect_now();
    if (v32::gc_used() != base + one) e++;

    // dropping the end pointer frees the array
    g_end = 0;
    collect_now();
    if (v32::gc_used() != base) e++;

    // an array of objects with a destructor: elements one past the end
    make_objects();
    collect_now();
    churn();
    if (g_dend[-1].id != 5 || g_dend[-3].id != 5) e++;
    g_dend = 0;
    collect_now();
    if (v32::gc_used() != base) e++;

    test_errors = e;
    return 0;
}
