// *****************************************************************************
//  tests/101sample.cpp — structs passed and returned BY VALUE
//
//  Vircon32 C moves one word per parameter and per return value ("functions
//  cannot return values of size > 1" / "cannot pass arguments of size > 1").
//  v32c++ rewrites a larger struct to travel by address (lower.c phase 9b):
//  a hidden result pointer, and a pointer the callee copies from on entry.
//   1. plain C: return into a declaration, an assignment, a member, an array
//      element, through a pointer; nested and chained calls; results used
//      inside expressions; a discarded result;
//   2. by-value parameters stay by value (the callee's changes don't leak),
//      including when the argument aliases the destination (a = add(a, a));
//   3. evaluation order: conditions of while/for, the right side of && / ||,
//      a ternary's untaken branch;
//   4. function pointers and typedefs of them;
//   5. C++: methods, const methods, virtual methods, operators;
//   6. structs with nested struct and array members; one-word structs are
//      left alone.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

int test_errors = -1;

typedef struct Vec { int x; int y; } Vec;
struct Rect { Vec min; Vec max; };
struct Row { int cells[ 3 ]; int tag; };
struct One { int v; };

int g_calls = 0;

Vec make( int x, int y ) { Vec v; v.x = x; v.y = y; g_calls++; return v; }
Vec add( Vec a, Vec b ) { return make( a.x + b.x, a.y + b.y ); }
Vec scaled( Vec v, int k ) { v.x *= k; v.y *= k; return v; }   // modifies its copy
Vec pick( int c, Vec a, Vec b ) { return c ? a : b; }
Rect bounds( Vec a, Vec b ) { Rect r; r.min = a; r.max = b; return r; }
Row fill( int base )
{
    Row r;
    int i;
    for( i = 0; i < 3; i++ ) r.cells[ i ] = base + i;
    r.tag = base * 10;
    return r;
}
One one( int v ) { One o; o.v = v; return o; }
int sum( Vec v ) { return v.x + v.y; }
int area( Rect r ) { return ( r.max.x - r.min.x ) * ( r.max.y - r.min.y ); }
Vec fact_pair( int n )                       // recursion
{
    if( n <= 1 ) return make( 1, 1 );
    Vec prev = fact_pair( n - 1 );
    return make( prev.x * n, prev.y + 1 );
}

Vec g_origin = { 100, 200 };
Vec origin() { return g_origin; }

typedef Vec (*VecOp)( Vec a, Vec b );
Vec sub( Vec a, Vec b ) { return make( a.x - b.x, a.y - b.y ); }
Vec apply( VecOp op, Vec a, Vec b ) { return op( a, b ); }

class Body
{
public:
    Vec pos;
    Vec vel;
    Body() { pos = make( 0, 0 ); vel = make( 1, 2 ); }
    Vec where() const { return pos; }
    Vec ahead( int steps ) const { return add( pos, scaled( vel, steps ) ); }
    virtual Vec step() { pos = add( pos, vel ); return pos; }
    void moveTo( Vec p ) { pos = p; }
};

class Fast : public Body
{
public:
    Fast() : Body() { }
    virtual Vec step() { pos = add( pos, scaled( vel, 3 ) ); return pos; }
};

struct V2
{
    int x; int y;
    V2 operator+( V2 o ) const { V2 r; r.x = x + o.x; r.y = y + o.y; return r; }
    V2 operator*( int k ) const { V2 r; r.x = x * k; r.y = y * k; return r; }
};

void main()
{
    int e = 0;

    // 1
    Vec a = make( 1, 2 );
    Vec b;
    b = make( 3, 4 );
    Vec c = add( a, b );
    if( c.x != 4 || c.y != 6 ) e++;
    if( make( 5, 6 ).x + add( a, make( 1, 1 ) ).y != 8 ) e++;
    if( sum( add( add( a, b ), make( 10, 20 ) ) ) != 40 ) e++;
    Rect r = bounds( a, add( a, make( 4, 5 ) ) );
    if( area( r ) != 20 || area( bounds( make( 0, 0 ), make( 3, 3 ) ) ) != 9 ) e++;
    Vec list[ 3 ];
    list[ 1 ] = make( 7, 8 );
    r.min = make( -1, -2 );
    Vec* p = &list[ 2 ];
    *p = add( list[ 1 ], r.min );
    if( list[ 1 ].y != 8 || r.min.x != -1 || list[ 2 ].x != 6 || p->y != 6 ) e++;
    int before = g_calls;
    make( 9, 9 );                                    // result discarded, call kept
    if( g_calls != before + 1 ) e++;
    const Vec fixed = make( 11, 12 );
    if( sum( fixed ) != 23 ) e++;
    Row row = fill( 5 );
    if( row.cells[ 0 ] + row.cells[ 2 ] != 12 || row.tag != 50 || fill( 2 ).cells[ 1 ] != 3 ) e++;
    if( one( 42 ).v != 42 ) e++;
    Vec f = fact_pair( 5 );
    if( f.x != 120 || f.y != 5 ) e++;
    Vec o = origin();
    o.x = 0;
    if( g_origin.x != 100 || origin().y != 200 ) e++;

    // 2
    Vec s = scaled( a, 10 );
    if( s.x != 10 || s.y != 20 || a.x != 1 || a.y != 2 ) e++;
    a = add( a, a );
    if( a.x != 2 || a.y != 4 ) e++;
    a = scaled( a, 2 );
    if( a.x != 4 || a.y != 8 ) e++;

    // 3
    int loops = 0;
    Vec w = make( 0, 0 );
    while( add( w, make( 1, 0 ) ).x <= 3 ) { w = add( w, make( 1, 0 ) ); loops++; }
    if( loops != 3 || w.x != 3 ) e++;
    int i;
    for( i = 0; sum( make( i, i ) ) < 6; i++ ) { }
    if( i != 3 ) e++;
    before = g_calls;
    if( a.x == 999 && make( 1, 1 ).x == 1 ) e++;     // right side must not run
    if( a.x == 4 || make( 1, 1 ).x == 1 ) { } else e++;
    if( g_calls != before ) e++;
    Vec t = pick( 1, a, make( 50, 50 ) );            // argument IS evaluated
    if( t.x != 4 || g_calls != before + 1 ) e++;
    int tern = ( a.x > 100 ) ? make( 1, 1 ).x : 7;   // untaken branch must not run
    if( tern != 7 || g_calls != before + 1 ) e++;

    // 4
    VecOp op = add;
    Vec viaptr = op( make( 1, 1 ), make( 2, 2 ) );
    if( viaptr.x != 3 ) e++;
    op = sub;
    if( apply( op, make( 9, 9 ), make( 4, 5 ) ).y != 4 ) e++;
    if( apply( add, a, a ).x != 8 ) e++;

    // 5
    Body body;
    Vec where = body.where();
    if( where.x != 0 || body.ahead( 4 ).y != 8 ) e++;
    body.moveTo( make( 10, 10 ) );
    Vec stepped = body.step();
    if( stepped.x != 11 || body.pos.y != 12 ) e++;
    Fast fast;
    Body* bp = &fast;
    Vec fs = bp->step();
    if( fs.x != 3 || fs.y != 6 || bp->step().x != 6 ) e++;
    V2 u; u.x = 1; u.y = 2;
    V2 v; v.x = 10; v.y = 20;
    V2 sum2 = u + v * 2;
    if( sum2.x != 21 || sum2.y != 42 || ( u + u ).y != 4 ) e++;

    test_errors = e;
}
