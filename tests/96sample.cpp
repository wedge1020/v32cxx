// *****************************************************************************
//  tests/96sample.cpp — DELIBERATELY INVALID: an array size that is not an
//  integer constant expression.
//
//  Array dimensions accept any integer constant expression now -- a
//  literal, a #define, an enum constant, or arithmetic on those (see
//  sample95). A runtime value is still not one, in C++ or in Vircon32 C,
//  so this must fail at parse time with a clear message naming the line,
//  not slip through to the downstream compiler.
// *****************************************************************************

#define BASE 4

int make_buffer( int n )
{
    int fine[ BASE * 2 ];        // constant: accepted
    int bad[ n + BASE ];         // runtime value: rejected (line 17)
    return 0;
}

void main()
{
    make_buffer( 3 );
}
