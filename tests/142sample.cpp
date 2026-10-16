/* tests/142sample.cpp -- expected to FAIL: a value returned from a void function */

void f ( int x )
{
    if ( x )
        return (1);
}

void main ( void )
{
    f ( 1 );
}
