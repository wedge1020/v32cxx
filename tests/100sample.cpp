// *****************************************************************************
//  tests/100sample.cpp — plain-C constructs that Vircon32 C itself lacks or is
//  stricter about, found by demos/c/spyvsspy and its gap_probes.cpp
//
//   1. arrays as operands: `arr + n`, `p - arr`, `p == arr`, `*arr`,
//      `(int)arr` -- Vircon32 C only decays an array in an assignment,
//      initializer or argument, so these become `&arr[0]`;
//   2. reading const values: `n = c;`, `f( c )`, `return c;`, `n = -c;`,
//      members and elements reached through a pointer-to-const, const
//      struct copies -- all "discards const qualifier" downstream;
//   3. storage classes: file-scope static (variable and function), static
//      locals (kept between calls, per-function), extern + definition,
//      volatile;
//   4. unsigned / signed / short / long spellings, u/l literal suffixes;
//   5. `T* const`, `%=`, comma operator in a return;
//   6. declarators: `int a, b[4];`, `int t[] = {...};`;
//   7. braced initializers for structs, nested arrays, arrays of structs;
//   8. 0 / NULL with function pointers, `#ifndef NULL` fallback.
//   9. bit-fields, accepted as full-word members (with a warning; an error
//      under --reject-bit-fields).
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

#ifndef NULL
#define NULL 0
#endif

int test_errors = -1;

struct Vec { int x; int y; };
struct Box { Vec min; Vec max; int tags[ 2 ]; };
typedef struct Vec Vec;
typedef int (*IntFn)( int v );

// --- 3: storage classes -------------------------------------------------
extern int g_shared;                 // declared here...
static int s_calls = 0;
volatile int v_ticks = 0;
int g_shared = 40;                   // ...defined here: one object

static int twice( int x ) { return x * 2; }

int next_id()
{
    static int id = 100;             // survives between calls
    id += 1;
    return id;
}

int other_counter()
{
    static int id;                   // a different `id`, zero-initialized
    static int step = 3;
    id += step;
    {
        int id = 1000;               // shadows the static inside this block
        id += 1;
    }
    return id;
}

// --- 4: integer spellings -----------------------------------------------
unsigned int  u_big   = 4000000000u;
signed int    s_neg   = -5;
short         h_val   = 300;
long          l_val   = 100000L;
unsigned char uc_val  = 200;
long long     ll_val  = 7;
unsigned      u_plain = 9;

// --- 2: const reads -----------------------------------------------------
const int K = 6;
const int TABLE[ 3 ] = { 10, 20, 30 };

int pass( int v ) { return v; }
int clampi( const int v, const int lo, const int hi )
{
    return v < lo ? lo : ( v > hi ? hi : v );
}
int ident( const int v ) { return v; }
int sum_vec( const Vec* v )
{
    int s;
    s = v->x;
    s += pass( v->y );
    return s;
}
int first_of( const int* a ) { int r; r = a[ 0 ]; r = *a; return a[ 1 ] + r; }
int neg_of( const int v ) { int r; r = -v; return r; }
int copy_x( const Vec* v ) { Vec local; local = *v; return local.x; }

// --- 5 ------------------------------------------------------------------
int bump_and_add( int a, int b ) { return a += 1, a + b; }
int add3( int v ) { return v + 3; }

// --- 9: bit-fields ------------------------------------------------------
struct Packed
{
    int flag  : 1;
    int level : 6;
    int spare : 9;
    int plain;
};

// --- 1: arrays as operands ----------------------------------------------
int  g_nums[ 8 ];
Vec  g_vecs[ 4 ];
int  g_grid[ 2 ][ 3 ] = { { 1, 2, 3 }, { 4, 5, 6 } };
Vec  g_table[ 2 ] = { { 1, 2 }, { 3, 4 } };

int array_param( int a[], int b[ 4 ] ) { return a[ 1 ] + b[ 3 ]; }

void main( void )
{
    int e = 0;
    int i;

    // 1
    for( i = 0; i < 8; i++ ) g_nums[ i ] = i * 10;
    int* p   = g_nums + 4;
    int* q   = 2 + g_nums;
    int* end = g_nums + 8;
    Vec* vend = g_vecs + 4;
    if( *p != 40 || *q != 20 ) e++;
    if( p - g_nums != 4 || end - p != 4 ) e++;
    if( (int)( p - q ) != 2 ) e++;
    if( q == g_nums || !( g_nums == q - 2 ) ) e++;
    if( *g_nums != 0 || *( g_nums + 3 ) != 30 ) e++;
    int count = 0;
    Vec* it;
    for( it = g_vecs; it != vend; ++it ) { it->x = count; count++; }
    if( count != 4 || g_vecs[ 3 ].x != 3 || vend - g_vecs != 4 ) e++;
    if( sizeof( g_nums ) != 8 || sizeof g_vecs != 8 ) e++;       // no decay in sizeof
    if( array_param( g_nums, g_nums + 2 ) != 60 ) e++;

    // 2
    const int c = 7;
    int n;
    n = c;                        if( n != 7 ) e++;
    n = pass( c );                if( n != 7 ) e++;
    n = ident( c );               if( n != 7 ) e++;
    n = K;                        if( n != 6 ) e++;
    n = TABLE[ 1 ];               if( n != 20 ) e++;
    n = neg_of( c );              if( n != -7 ) e++;
    if( clampi( 50, 0, 9 ) != 9 || clampi( -3, 0, 9 ) != 0 || clampi( 4, 0, 9 ) != 4 ) e++;
    Vec v = { 11, 22 };
    if( sum_vec( &v ) != 33 || copy_x( &v ) != 11 ) e++;
    if( first_of( g_nums ) != 10 ) e++;

    // 3
    s_calls += twice( 3 );
    v_ticks = v_ticks + 1;
    if( s_calls != 6 || v_ticks != 1 || g_shared != 40 ) e++;
    if( next_id() != 101 || next_id() != 102 || next_id() != 103 ) e++;
    if( other_counter() != 3 || other_counter() != 6 ) e++;

    // 4
    if( s_neg != -5 || h_val != 300 || l_val != 100000 || uc_val != 200 ) e++;
    if( ll_val != 7 || u_plain != 9 ) e++;
    if( (int)u_big != -294967296 ) e++;
    unsigned int local_u = 12u;
    long int local_l = 30l;
    if( (int)( local_u + (unsigned int)local_l ) != 42 ) e++;

    // 5
    int* const locked = &g_nums[ 5 ];
    if( *locked != 50 ) e++;
    n = 23; n %= 7;               if( n != 2 ) e++;
    if( bump_and_add( 4, 10 ) != 15 ) e++;

    // 6
    int a, b[ 4 ], *r, m[ 2 ][ 2 ];
    int t[] = { 10, 20, 30 };
    int lone[ 2 ], after = 5;
    a = 1; b[ 3 ] = 2; r = &a; m[ 1 ][ 1 ] = 3; lone[ 1 ] = 4;
    if( a + b[ 3 ] + *r + m[ 1 ][ 1 ] + lone[ 1 ] + after != 16 ) e++;
    if( sizeof( t ) != 3 || t[ 0 ] + t[ 1 ] + t[ 2 ] != 60 ) e++;

    // 7
    Box box = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
    int grid[ 2 ][ 2 ] = { { 1, 2 }, { 3, 4 }, };
    Vec w;
    w = v;
    if( box.min.y + box.max.x + box.tags[ 1 ] != 11 ) e++;
    if( grid[ 1 ][ 0 ] != 3 || g_grid[ 1 ][ 2 ] != 6 || g_table[ 1 ].x != 3 ) e++;
    if( w.x != 11 || w.y != 22 ) e++;

    // 8
    IntFn fn;
    int* np = NULL;
    fn = 0;
    if( fn != 0 ) e++;
    fn = add3;
    if( fn == 0 || fn( 4 ) != 7 || np != 0 ) e++;

    // 9
    Packed pk;
    pk.flag = 1; pk.level = 33; pk.spare = 0; pk.plain = 5;
    if( pk.flag + pk.level + pk.plain != 39 || sizeof( Packed ) != 4 ) e++;

    test_errors = e;
}
