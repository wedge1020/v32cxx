// *****************************************************************************
//  tests/182sample.cpp -- `--alloc=gc`
//
//  Built with --alloc=gc (std_run adds --heap-words=16384: a heap of 64 KB as
//  standard C). new/new[]/std::vector take their space from one conservative
//  mark-sweep heap. Checked here:
//    - an unreachable cycle is collected (gc_used goes back to its baseline)
//    - objects reachable only from a global, from a stack local of a caller,
//      through an interior pointer (into a `new int[10]`, to a member) or only
//      through a std::vector buffer survive a collection with their contents
//    - delete frees at once and runs the destructor; collected objects get none
//    - a loop that allocates ~100 times the heap while keeping little alive
//    - collections started by the threshold (not by gc_collect()) -- skipped
//      when the sample is built with -DGC_NO_THRESHOLD (then only a full heap
//      starts one)
//    - a mark stack overflow (700 pointers in one buffer, 512 mark entries)
//
//  A conservative collector can keep a "dead" object alive through a stale
//  stack slot, so nothing here depends on a dead object being found dead
//  except the cycle and the baseline, and those objects are made in helper
//  functions that return, with scrub() overwriting the dead stack below the
//  caller before every collection. Flakiness risk: a stale copy of a pointer in
//  a callee-saved register or in a stack word scrub() does not reach would keep
//  the cycle (gc_used > baseline); it did not happen at -O0.
// *****************************************************************************
#include <v32/memory.hpp>
#include <vector>

int test_errors = -1;
int e = 0;
int g_dtors = 0;

class Node {
public:
    Node *next;
    int id;
    int pad[6];
    Node(int i)
    {
        id = i;
        next = 0;
        for (int k = 0; k < 6; k++) pad[k] = i * 10 + k;
    }
};

class Leaf {
public:
    int v;
    Leaf(int x) { v = x; }
};

class Holder {
public:
    Node *held;
    int n;
};

class D {
public:
    int id;
    D() { id = 0; }
    D(int i) { id = i; }
    ~D() { g_dtors = g_dtors + 1; }
};

Node *g_keep = 0;
int *g_mid = 0;
int *g_member = 0;
Node *g_ring[50];
Holder g_holder;
std::vector<int> g_vec;
std::vector<Node *> g_nodes;
std::vector<Leaf *> g_leaves;

// overwrites the dead stack below the caller, where stale pointers of finished
// calls (and of the collector's own earlier frames) may still sit
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

// allocates (and drops) enough to reuse any block a bad collection freed
void churn()
{
    for (int i = 0; i < 300; i++) {
        Node *t = new Node(-1);
        t->pad[0] = -1;
    }
}

void make_cycle()
{
    Node *a = new Node(1);
    Node *b = new Node(2);
    a->next = b;
    b->next = a;
}

void make_garbage(int count)
{
    for (int i = 0; i < count; i++) {
        Node *n = new Node(i);
        n->next = 0;
    }
}

Node *make_chain(int n)
{
    Node *head = 0;
    for (int i = n; i > 0; i--) {
        Node *c = new Node(i);
        c->next = head;
        head = c;
    }
    return head;
}

void make_global()
{
    g_keep = make_chain(4);
    g_holder.held = make_chain(3);
    g_holder.n = 3;
    g_ring[7] = make_chain(2);
}

void make_interior()
{
    int *a = new int[10];
    for (int i = 0; i < 10; i++) a[i] = 100 + i;
    g_mid = &a[5];
    Node *o = new Node(30);
    o->pad[0] = 5;
    o->pad[2] = 77;
    g_member = &o->pad[2];
}

void fill_vectors()
{
    for (int i = 0; i < 100; i++) g_vec.push_back(i * 3);
    for (int i = 0; i < 20; i++) g_nodes.push_back(new Node(500 + i));
    for (int i = 0; i < 700; i++) g_leaves.push_back(new Leaf(i + 9000));
}

int chain_sum(Node *n)
{
    int s = 0;
    while (n != 0) {
        s = s + n->id + n->pad[3];
        n = n->next;
    }
    return s;
}

// the only reference is a local of this frame, the caller of gc_collect()
int stack_local_survives()
{
    Node *loc = make_chain(5);
    std::vector<int> lv;
    for (int i = 0; i < 40; i++) lv.push_back(i + 1);
    int want = 0;
    for (int i = 1; i <= 5; i++) want = want + i + (i * 10 + 3);
    collect_now();
    churn();
    int bad = 0;
    if (chain_sum(loc) != want) bad++;
    for (int i = 0; i < 40; i++)
        if (lv[i] != i + 1) bad++;
    return bad;
}

void ring_pressure()
{
    for (int i = 0; i < 20000; i++) {
        Node *n = new Node(i);
        int *big = new int[40];
        big[0] = i;
        big[39] = i;
        g_ring[i % 50] = n;
        if (big[0] != big[39]) e++;
    }
}

void delete_tests()
{
    collect_now();
    int u0 = v32::gc_used();
    Node *t = 0;
    t = new Node(1);
    int u1 = v32::gc_used();
    delete t;
    int u2 = v32::gc_used();
    if (u1 <= u0 || u2 != u0) e++;          // delete frees at once, no collection needed

    int d0 = g_dtors;
    D *a = 0;
    D *b = 0;
    D *arr = 0;
    a = new D(1);
    b = new D(2);
    delete a;
    if (g_dtors != d0 + 1) e++;
    arr = new D[3];
    delete[] arr;
    if (g_dtors != d0 + 4) e++;
    delete b;
    if (g_dtors != d0 + 5) e++;
    // the freed blocks are reused, and a collection after the deletes leaves nothing behind
    for (int i = 0; i < 50; i++) {
        D *x = new D(i);
        delete x;
    }
    if (g_dtors != d0 + 55) e++;
    collect_now();
    if (v32::gc_used() != u0) e++;
}

void drop_ds()
{
    D *x = new D(7);
    D *y = new D[4];
    x->id = y[1].id + 1;
}

int main(void)
{
    int i;
    for (i = 0; i < 50; i++) g_ring[i] = 0;
    if (v32::gc_free() <= 0 || v32::gc_used() < 0) e++;

    // an unreachable cycle is collected: usage returns to what it was
    collect_now();
    int base = v32::gc_used();
    int c0 = v32::gc_collections();
    make_cycle();
    make_cycle();
    if (v32::gc_used() <= base) e++;
    collect_now();
    if (v32::gc_used() != base) e++;
    if (v32::gc_collections() != c0 + 1) e++;

    // reachable from a global (a pointer, a member of a global object, an element of a global array)
    make_global();
    make_interior();
    fill_vectors();
    collect_now();
    churn();
    if (chain_sum(g_keep) != (1 + 13) + (2 + 23) + (3 + 33) + (4 + 43)) e++;
    if (chain_sum(g_holder.held) != (1 + 13) + (2 + 23) + (3 + 33)) e++;
    if (chain_sum(g_ring[7]) != (1 + 13) + (2 + 23)) e++;

    // reachable only through an interior pointer
    if (g_mid[0] != 105 || g_mid[-5] != 100 || g_mid[4] != 109 || g_mid[-3] != 102) e++;
    if (g_member[0] != 77 || g_member[-2] != 5) e++;

    // reachable only through a std::vector buffer (numbers, and objects named by its pointers)
    for (i = 0; i < 100; i++)
        if (g_vec[i] != i * 3) e++;
    for (i = 0; i < 20; i++)
        if (g_nodes[i]->id != 500 + i || g_nodes[i]->pad[3] != (500 + i) * 10 + 3) e++;
    for (i = 0; i < 700; i++)
        if (g_leaves[i]->v != i + 9000) e++;       // 700 > 512: the mark stack overflowed

    // reachable only from a local of a caller
    if (stack_local_survives() != 0) e++;

    // delete, and collected objects get no destructor (scrub first: the uninitialized
    // slots of delete_tests' frame would otherwise still hold the finished calls' pointers)
    scrub();
    delete_tests();
    int d1 = g_dtors;
    drop_ds();
    collect_now();
    if (g_dtors != d1) e++;

    // far more than the heap is allocated, little is kept alive
    int cc = v32::gc_collections();
    ring_pressure();
    if (v32::gc_collections() <= cc) e++;
    for (i = 0; i < 50; i++) {
        int last = 19950 + i;
        if (i == 7) continue;
        if (g_ring[last % 50]->id != last || g_ring[last % 50]->pad[5] != last * 10 + 5) e++;
    }
    if (g_ring[7] == 0) e++;

#ifndef GC_NO_THRESHOLD
    // nobody calls gc_collect(): the threshold (a quarter of the heap) starts collections
    collect_now();
    cc = v32::gc_collections();
    make_garbage(2000);
    if (v32::gc_collections() < cc + 2) e++;
#endif

    // everything that must survive still does after all that churn
    for (i = 0; i < 100; i++)
        if (g_vec[i] != i * 3) e++;
    if (chain_sum(g_keep) != (1 + 13) + (2 + 23) + (3 + 33) + (4 + 43)) e++;
    if (g_mid[0] != 105 || g_member[0] != 77) e++;

    test_errors = e;
    return 0;
}
