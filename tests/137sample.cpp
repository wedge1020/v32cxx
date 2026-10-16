// *****************************************************************************
//  tests/137sample.cpp -- --no-signed-shift
//
//  Built with --no-signed-shift (see the Makefile): `>>` and `>>=` are
//  printed as written, so a negative value shifts LOGICALLY, as Vircon32's
//  own shift does. Shows what the flag costs, and that it changes nothing
//  for non-negative values or `unsigned` ones.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

int main()
{
    int e = 0;
    int x = -8, p = 100, one = 1;
    unsigned u = 0x80000000u;
    int y = x;

    if( (x >> one) != 0x7FFFFFFC ) e += 1;     // logical: NOT -4
    y >>= one;
    if( y != 0x7FFFFFFC ) e += 2;
    if( (p >> 3) != 12 ) e += 4;
    if( (u >> 31) != 1 ) e += 8;
    if( (x << 1) != -16 ) e += 16;

    test_errors = e;
    return 0;
}
