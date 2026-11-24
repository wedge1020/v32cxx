// The gc heap runs out of space for real (built with --alloc=gc --heap-words=2048):
// everything allocated stays reachable (a linked list from a global), so a
// collection cannot help and the program must stop with a message, not carry on
// with a bad pointer. Before that, 20000 blocks are allocated and dropped, which
// only works if collection reclaims them. tests/gc_full.sh builds it as
// standard C and checks the exit status (1) and the message; "survived" must
// never print.
#include <v32/memory.hpp>

int test_errors = -1;

class Cell {
public:
    Cell *next;
    int words[6];
    Cell() { next = 0; words[0] = 1; }
};

Cell *g_head = 0;
int g_garbage = 0;

int main(void)
{
    for (int i = 0; i < 20000; i++) {
        Cell *c = new Cell;
        c->words[1] = i;
        g_garbage = g_garbage + c->words[0];
    }
    if (v32::gc_collections() == 0) return 1;
    for (int i = 0; i < 100000; i++) {
        Cell *c = new Cell;
        c->next = g_head;
        g_head = c;
    }
    test_errors = 0;
    return 0;
}
