// *****************************************************************************
//  tests/135sample.cpp -- ordinary C++ spellings of things Vircon32 C rejects
//
//  docs/VIRCON32_QUIRKS.md lists what Vircon32 C cannot take; v32c++ rewrites
//  every one below, so the SOURCE stays plain C++. Self-checking: each item
//  adds its own number to test_errors when it is wrong (0 = all fine).
// *****************************************************************************

int test_errors = -1;

int add3( int, int, int );                  // prototype without parameter names
int add3( int a, int b, int c ) { return a + b + c; }

int counter()
{
    static int calls = 0;                   // static local
    calls++;
    return calls;
}

struct Node { int v; Node *next; };

int main()
{
    int e = 0;
    int x = 5, y = 7;
    Node  n;
    Node *p = 0;                            // null pointer as 0
    float f = 1e3f + 1.5e-3f;               // exponent form
    float g = .5f + 5. + 1e2 + 2.e1f;       // .5  5.  1e2  2.e1: 125.5
    char  c = '\0';                         // NUL and octal escapes
    unsigned int u = 7u;
    long      big = 100000L;
    int  z = (x++, y++, x + y);             // comma operator (6 + 8)
    int  w = 0;

    n.v = 3;
    n.next = 0;

    if( p != 0 ) e += 1;
    if( n.next != 0 ) e += 2;
    if( c != 0 || '\101' != 'A' ) e += 4;
    if( f < 1000.0f || f > 1000.01f ) e += 8;
    if( z != 14 ) e += 16;
    if( add3( 1, 2, 3 ) != 6 ) e += 32;
    counter();
    if( counter() != 2 ) e += 64;
    if( g < 125.4f || g > 125.6f ) e += 1024;
    if( u != 7 || big != 100000 ) e += 128;

    switch( x )
    {
        case 6:
        {
            int inner = 9;                  // declaration after a case label
            w = inner;
            break;
        }
        default:
            break;
    }
    if( w != 9 ) e += 256;

    for( int i = 0, j = 3; i < j; i++, j-- )   // comma in a for clause
      w++;
    if( w != 11 ) e += 512;

    test_errors = e;
    return 0;                               // int main()
}
