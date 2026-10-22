/* tests/154sample.c -- `T * b;` with a type v32c++ does not know.
 * Built with no SDK header findable (the Makefile passes no -I and the
 * default SDK directory does not exist here): v32c++ cannot know
 * malloc_block, reads the declaration as a multiplication, and says so
 * -- twice: the SDK header was not found, and the statement has no effect. */
#include "misc.h"

int f (void)
{
    malloc_block *b;
    int s = 0;
    for (b = malloc_first_block; (int) b != -1; b = b->next)
        s += b->size;
    return s;
}

int main (void) { return f (); }
