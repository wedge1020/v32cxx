/* *****************************************************************************
 *  tests/152sample.cpp -- members of a struct defined in an SDK header
 *
 *  misc.h defines `struct malloc_block { malloc_block* previous; ...; int
 *  size; bool free; }`. v32c++ reads a plain C struct like that from the
 *  header it passes through, so b->size, b->next and sizeof work on it like
 *  on any struct of the program's own (they used to be an error: "member
 *  access on native type"). The header still defines the struct downstream:
 *  the generated C must not define it a second time. A struct the reader
 *  cannot follow (a function-pointer member) stays an opaque `native` type.
 *  Self-checking: test_errors must end at 0.
 * ***************************************************************************** */

#include "misc.h"

int test_errors = -1;

void main (void)
{
    int e = 0;
    int blocks = 0;

    void *p1 = malloc (10);
    void *p2 = malloc (20);
    void *p3 = malloc (30);

    malloc_block *b = malloc_first_block;
    while (b != NULL)
    {
        blocks++;
        b = b->next;
    }
    if (blocks < 3) e += 1;
    if (sizeof (malloc_block) != 4) e += 2;

    malloc_block local;
    local.size = 7;
    local.next = &local;
    if (local.next->size != 7) e += 4;

    free (p1);
    free (p2);
    free (p3);
    test_errors = e;
}
