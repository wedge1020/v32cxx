// *****************************************************************************
//  tests/181sample.cpp -- `--alloc=arena`
//
//  Built with --alloc=arena. new/new[]/std::vector take their space from one
//  arena; arena_release() gives it back and runs the destructors of the
//  objects still alive, newest first; `delete` runs the destructor at once and
//  release does not run it again. A constructor logs its id, a destructor the
//  negated id. test_errors must end at 0.
// *****************************************************************************
#include <v32/memory.hpp>
#include <vector>

int test_errors = -1;
int g_log[64];
int g_n = 0;
int g_live = 0;

class Node {
public:
    int id;
    Node() { id = 9; g_log[g_n] = 9; g_n = g_n + 1; g_live = g_live + 1; }
    Node(int i) { id = i; g_log[g_n] = i; g_n = g_n + 1; g_live = g_live + 1; }
    ~Node() { g_log[g_n] = 0 - id; g_n = g_n + 1; g_live = g_live - 1; }
};

// no destructor: no record is kept, nothing runs on release
class Plain {
public:
    int v;
    Plain(int x) { v = x; }
};

int e = 0;

void check(const int *want, int n)
{
    if (g_n != n) e++;
    for (int i = 0; i < n && i < g_n; i++)
        if (g_log[i] != want[i]) e++;
    g_n = 0;
}

int main(void)
{
    int base = v32::arena_used();
    if (base < 0) e++;
    if (v32::arena_free() <= 0) e++;

    // mark / release: destructors of what is left, newest first
    int m = v32::arena_mark();
    Node *a = new Node(1);
    Node *b = new Node(2);
    Plain *p = new Plain(5);
    if (p->v != 5 || a->id != 1 || b->id != 2) e++;
    if (v32::arena_used() <= base) e++;
    {   int w[] = { 1, 2 };
        check(w, 2);
    }
    v32::arena_release(m);
    {   int w[] = { -2, -1 };
        check(w, 2);
    }
    if (v32::arena_used() != base) e++;

    // delete runs the destructor now; release must not run it again
    m = v32::arena_mark();
    a = new Node(3);
    b = new Node(4);
    delete a;
    {   int w[] = { 3, 4, -3 };
        check(w, 3);
    }
    v32::arena_release(m);
    {   int w[] = { -4 };
        check(w, 1);
    }
    if (g_live != 0) e++;
    if (v32::arena_used() != base) e++;

    // arrays: elements built in order, destroyed last first, by delete[] and by release
    m = v32::arena_mark();
    Node *arr = new Node[3];
    Node *arr2 = new Node[2];
    delete[] arr;
    {   int w[] = { 9, 9, 9, 9, 9, -9, -9, -9 };
        check(w, 8);
    }
    v32::arena_release(m);
    {   int w[] = { -9, -9 };
        check(w, 2);
    }
    if (g_live != 0) e++;
    if (v32::arena_used() != base) e++;
    (void)arr2;

    // a vector growing inside an arena (the old buffers stay until release)
    m = v32::arena_mark();
    std::vector<int> v;
    for (int i = 0; i < 100; i++) v.push_back(i * 2);
    int sum = 0;
    for (int i = 0; i < 100; i++) sum += v[i];
    if (sum != 9900 || v.size() != 100) e++;
    if (v32::arena_used() <= base) e++;
    // (std::string keeps its characters inside the object: no heap, nothing to
    // route -- tests/181string.cpp builds one under --alloc=arena)
    v32::arena_release(m);
    // `v` itself is a local: its destructor releases into the (no-op) hook
    if (v32::arena_used() != base) e++;

    // reset frees everything
    Node *c = new Node(7);
    (void)c;
    v32::arena_reset();
    {   int w[] = { 7, -7 };
        check(w, 2);
    }
    if (v32::arena_used() != 0) e++;

    test_errors = e;
    return 0;
}
