// *****************************************************************************
//  tests/186gc.cpp -- `--alloc=gc`: roots inside a namespace
//
//  A pointer, an object holding pointers and an array of pointers declared in a
//  namespace (nested too) are roots of the collector like file-scope ones:
//  what they alone refer to survives a collection. An object built in a
//  namespace is constructed at the start of main (so its pointer member is set
//  before anything runs). Built with --alloc=gc --heap-words=16384 by
//  `make test`; test_errors must end at 0.
// *****************************************************************************
#include <v32/memory.hpp>

int test_errors = -1;
int e = 0;

class Node {
public:
    Node *next;
    int id;
    int pad[6];
    Node(int i) { id = i; next = 0; for (int k = 0; k < 6; k++) pad[k] = i * 10 + k; }
};

class Owner {
public:
    Node *held;
    int tag;
    Owner(int t) { tag = t; held = new Node(t); }
};

namespace app {
    Node *kept = 0;
    Node *slots[3];
    Owner owner(40);                    // constructed before main's body, allocates
    namespace deep {
        Node *tail = 0;
    }
}

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

void churn()
{
    for (int i = 0; i < 300; i++) {
        Node *t = new Node(-1);
        t->pad[0] = -1;
    }
}

void fill()
{
    app::kept = new Node(1);
    app::slots[1] = new Node(2);
    app::deep::tail = new Node(3);
    app::kept->next = new Node(4);
}

int main(void)
{
    fill();
    collect_now();
    churn();
    if (app::kept->id != 1 || app::kept->pad[3] != 13) e++;
    if (app::kept->next == 0 || app::kept->next->id != 4 || app::kept->next->pad[5] != 45) e++;
    if (app::slots[1]->id != 2 || app::slots[1]->pad[2] != 22) e++;
    if (app::deep::tail->id != 3 || app::deep::tail->pad[4] != 34) e++;
    if (app::owner.tag != 40 || app::owner.held->id != 40 || app::owner.held->pad[1] != 401) e++;

    // dropped from the namespace-scope root: the heap shrinks back
    int used = v32::gc_used();
    app::deep::tail = 0;
    app::slots[1] = 0;
    collect_now();
    if (v32::gc_used() >= used) e++;

    test_errors = e;
    return 0;
}
