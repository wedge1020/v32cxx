// *****************************************************************************
//  tests/136sample.cpp -- right shift of a signed value
//
//  Vircon32's shift is logical: -8 >> 1 would be 0x7FFFFFFC. v32c++ rewrites a
//  `>>` or `>>=` of a SIGNED value into an arithmetic shift, as in C, and
//  leaves an `unsigned` one alone (see docs/VIRCON32_QUIRKS.md #14).
//  Self-checking: each failing item adds its own number to test_errors.
// *****************************************************************************

typedef unsigned u32;

int test_errors = -1;

int      g_neg   = -20;                 // a global
int      g_table[ 4 ];
int      g_index = 0;
unsigned g_ugl   = 0x80000000u;

struct Pair { int a; int b; unsigned u; };

int minus_forty() { return -40; }       // a call: goes through the helper
int next_index()  { g_index++; return g_index - 1; }

class Box
{
public:
    int v;
    int half() { return v >> 1; }       // a method: member of this
    void shrink() { v >>= 2; }
};

int main()
{
    int e = 0;
    int x = -8, one = 1, zero = 0, n = 3;
    int big = -2147483647 - 1;          // INT_MIN
    int pos = 100;
    Pair p;
    Pair *pp = &p;
    int *ip = &x;
    Box box;
    unsigned u = 0x80000000u;
    u32      w = 0xF0000000u;
    int count = 0;

    p.a = -33; p.b = 77; p.u = 0xFFFFFFF0u;
    g_table[ 0 ] = -100; g_table[ 1 ] = 100;
    box.v = -64;

    // plain variables
    if( (x >> 1) != -4 ) e += 1;
    if( (x >> 2) != -2 ) e += 2;
    if( ((x - 1) >> 1) != -5 ) e += 4;            // -9 >> 1 rounds down
    if( (x >> 31) != -1 ) e += 8;
    if( (x >> zero) != -8 ) e += 16;
    if( (-1 >> n) != -1 ) e += 32;                // constant folded
    if( (big >> 31) != -1 ) e += 64;
    if( (big >> 1) != -1073741824 ) e += 128;
    if( (pos >> 3) != 12 ) e += 256;              // positive: as before
    if( (x >> n) != -1 ) e += 512;                // variable count

    // other kinds of operand
    if( (g_neg >> 2) != -5 ) e += 1024;
    if( (g_table[ 0 ] >> 3) != -13 ) e += 2048;   // -100 / 8 rounds down
    if( (p.a >> 1) != -17 ) e += 4096;
    if( (pp->a >> 2) != -9 ) e += 8192;
    if( (*ip >> 1) != -4 ) e += 16384;
    if( (minus_forty() >> 3) != -5 ) e += 32768;  // call: the helper
    if( ((x + pos - 108) >> 1) != -8 ) e += 65536;     // an expression: the helper
    if( (box.half()) != -32 ) e += 131072;

    // compound assignment
    {
        int a = -64, c = 5;
        a >>= 2;
        if( a != -16 ) e += 262144;
        a >>= n;
        if( a != -2 ) e += 524288;
        c >>= 1;
        if( c != 2 ) e += 1048576;
        g_neg >>= 2;
        if( g_neg != -5 ) e += 2097152;
        p.a >>= 1;
        if( p.a != -17 ) e += 4194304;
        box.shrink();
        if( box.v != -16 ) e += 8388608;
    }

    // a side effect in the target happens once
    g_index = 0;
    g_table[ 2 ] = -32; g_table[ 3 ] = -32;
    g_table[ next_index() + 2 ] >>= 2;
    if( g_index != 1 ) e += 16777216;
    if( g_table[ 2 ] != -8 || g_table[ 3 ] != -32 ) e += 33554432;

    // unsigned stays logical
    if( (u >> 4) != 0x08000000 ) e += 67108864;
    if( (w >> 28) != 15 ) e += 134217728;
    if( (g_ugl >> 31) != 1 ) e += 268435456;
    if( (p.u >> 28) != 15 ) e += 536870912;

    test_errors = e;

    // the second group, added to the same counter (bits 30 and up are used
    // above, so these are small numbers in test_errors' own low range of a
    // second variable folded in at the end)
    int f = 0;
    u >>= 4;
    if( u != 0x08000000 ) f += 1;
    if( ((u * 3u) >> 1) != 0x0C000000 ) f += 2;   // unsigned arithmetic keeps it unsigned
    if( ((unsigned)x >> 28) != 15 ) f += 4;       // a cast to unsigned
    if( ((x & 0xFF) >> 4) != 15 ) f += 8;         // known non-negative
    if( (0x7F >> 3) != 15 ) f += 16;
    if( ((x == -8) >> 1) != 0 ) f += 32;
    if( ((x >> 4) & 15) != 15 ) f += 512;         // masked: the plain shift is enough
    if( ((x >> 1) & 0x7FFFFFFF) != 0x7FFFFFFC ) f += 1024;
    if( ((big >> 20) & 0xFFF) != 0x800 ) f += 2048;

    // a loop that halves a negative number down to -1
    int m = -37;
    while( m != -1 && count < 20 )
    {
        m >>= 1;
        count++;
    }
    if( count != 6 || m != -1 ) f += 64;

    // for-loop clause
    int k = 0;
    for( int q = -1024; q != -1; q >>= 2 )
      k++;
    if( k != 5 ) f += 128;

    // in a condition, a return, a call argument
    if( (x >> 1) == -4 && !((x >> 1) != -4) ) { } else f += 256;

    if( f != 0 ) test_errors = test_errors + 1073741824;   // flag the group in its own bit
    return 0;
}
