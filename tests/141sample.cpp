/* *****************************************************************************
 *  tests/141sample.cpp -- `return value;` in `void main`
 *
 *  Vircon32 wants `void main()`; a C++ habit is `return 0;`. The value is
 *  dropped (an expression with side effects still runs: here bump() clears test_errors).
 *  Self-checking: test_errors must end at 0.
 * ***************************************************************************** */

int test_errors = -1;

int bump ( void )
{
    test_errors = 0;      /* proves the call was kept */
    return 5;
}

void main ( void )
{
    return (bump ());
}
