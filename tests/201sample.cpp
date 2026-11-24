// *****************************************************************************
//  tests/201sample.cpp -- `extern` redeclaration of a variable an SDK header defines.
//
//  misc.h defines `void *malloc_start_address`, `malloc_end_address` and
//  `malloc_block *malloc_first_block`. Declaring them `extern` here (to name the
//  variable in the source) used to print a second definition: "identifier was
//  already declared". v32c++ reads the names of the header's variable
//  definitions (prescan.c) and leaves such redeclarations out of the output.
//
//  test_errors must end at 0.
// *****************************************************************************

#include "misc.h"

extern malloc_block *malloc_first_block;
extern void *malloc_start_address;
extern void *malloc_end_address;

int test_errors = -1;

int main(void)
{
    int e = 0;
    int *p = (int *)malloc(8);
    int *q = (int *)malloc(8);

    if (malloc_start_address != (void *)0x100000) e++;
    if (malloc_end_address != (void *)(3 * 1024 * 1024 - 1)) e++;
    if (malloc_first_block == NULL) e++;
    if ((void *)malloc_first_block != malloc_start_address) e++;
    int n = 0;
    for (malloc_block *b = malloc_first_block; b != NULL; b = b->next) n++;
    if (n < 3) e++;
    free(p);
    free(q);

    test_errors = e;
    return 0;
}
