/* *****************************************************************************
 *  tests/151sample.cpp -- #include inside a function body and an asm { } block
 *
 *  An #include met inside a { } body is text inclusion right there, whatever
 *  the file's extension (it used to be hoisted to the top of the output, out
 *  of the function, without a word). Here the lines of an asm block (raw
 *  words in ROM) and a run of statements come from files of their own.
 *  Self-checking: test_errors must end at 0. The Makefile also checks the
 *  generated C: the "integer" line sits inside the asm block and no
 *  #include of the .inc files reaches the output.
 * ***************************************************************************** */

int test_errors = -1;

int table_address (void)
{
    asm
    {
        "mov R0, __tbl"
        "jmp __tbl_end"
        "__tbl:"
        #include "151table.inc"
        "__tbl_end:"
    }
}

int forty_two (void)
{
    int total = 0;
#include "151body.inc"
    return total;
}

void main (void)
{
    int e = 0;
    int *t = (int *) table_address ();

    if (t[0] != 11) e += 1;
    if (t[1] != 22) e += 2;
    if (t[2] != 33) e += 4;
    if (forty_two () != 42) e += 8;
    test_errors = e;
}
