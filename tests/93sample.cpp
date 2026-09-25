// Deliberately INVALID: a qualified call names a namespace that does
// not declare the function. Before namespace-aware lookup, qualified
// calls were resolved by their final name alone, so v32::draw here
// silently bound to the file-scope draw. Now it is an error.
// Expected: exactly one "no function 'draw' is declared in namespace
// 'v32'" error, and a nonzero exit.

namespace v32
{
    int helper( int v ) { return v; }
}

int draw( int region, int x, int y ) { return 2; }

void main()
{
    int a = v32::draw( 0, 1, 2 );
}
