// *****************************************************************************
//  TEMPEST 32K -- src/math.cpp
//  math helpers: lerp, sine tables, hot-path inlining macros, RNG
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Math helpers
// ---------------------------------------------------------------------------
float lerp( float a, float b, float t )
{
    return a + ( b - a ) * t;
}

// HOT-PATH INLINING. The Vircon32 C compiler never inlines: every call
// costs argument pushes, a frame setup and a return before the body
// runs -- more than the body itself for one-liners like lerp or a table
// lookup. v32c++ expands function-like macros on the C++ side, so these
// give the hottest paths (project, the web, the HUD) call-free versions;
// the functions above and below stay for everything else.
#define LERP( a, b, t )   ( (a) + ( (b) - (a) ) * (t) )
#define TURN_INDEX( x )   ( ( (int)( (x) * 40.7436611 + 1024.5 ) ) & 255 )
#define SIN32( g, x )     ( (g)->SIN_TABLE[ TURN_INDEX( x ) ] )
#define COS32( g, x )     ( (g)->COS_TABLE[ TURN_INDEX( x ) ] )

float cos_taylor( float x );   // fwd: used by build_tables below

float sin_taylor( float x )
{
    // wrap to [-pi, pi] first
    while( x > 3.14159265 ) x -= 6.28318531;
    while( x < -3.14159265 ) x += 6.28318531;
    // THEN fold into [-pi/2, pi/2] via symmetry: the 4-term series is
    // excellent there (error ~2e-5) but GARBAGE near +/-pi, where it
    // gave sin(pi) = -0.075 instead of 0. That corrupted the tables
    // around index 128 (sin, due west) and 64 (cos, due south), which
    // tilted every west/south-pointing bar ~4.3 degrees — the east
    // spokes visibly missing the far cap, the bottom juts, misaligned
    // vertex endings. sin(x) = sin(pi - x) for x in (pi/2, pi];
    // sin(x) = sin(-pi - x) for x in [-pi, -pi/2).
    if( x > 1.57079632 )   x = 3.14159265 - x;
    if( x < -1.57079632 )  x = -3.14159265 - x;
    float x2 = x * x;
    return x * ( 1.0
          - x2 / 6.0
          + x2 * x2 / 120.0
          - x2 * x2 * x2 / 5040.0 );
}

void build_tables( G* g )
{
    int i;
    for( i = 0; i < 256; i++ )
    {
        float a = ( i * 3.14159265 * 2.0 ) / 256.0;
        g->SIN_TABLE[ i ] = sin_taylor( a );
        g->COS_TABLE[ i ] = cos_taylor( a );
    }
}

// Table index: ROUND to nearest entry, not truncate. (int) truncates
// toward zero, so negative angles landed 1 entry off (lane 0 vs lane
// 16 differed by 1.4 deg -> the top seam gap), and products landing
// near an integer boundary (lanes 5, 8) jittered by +-1 entry. The
// +1024 (four full turns) keeps the sum positive for every angle the
// game uses (|x| < 25 rad), so truncation direction never matters,
// and +0.5 rounds to nearest. 15.9999 and 16.0001 both give 16.
float sin32( G* g, float x )
{
    int i = ( (int)( x * 40.7436611 + 1024.5 ) ) & 255;
    return g->SIN_TABLE[ i ];
}

float cos_taylor( float x )
{
    return sin_taylor( x + 1.57079632 );
}

float cos32( G* g, float x )
{
    int i = ( (int)( x * 40.7436611 + 1024.5 ) ) & 255;
    return g->COS_TABLE[ i ];
}

float fabs_sin( G* g, float x )
{
    float s = sin32( g, x );
    if( s < 0 ) s = -s;
    return s;
}

int rng( G* g )
{
    g->rng_state = g->rng_state * 1103515245 + 12345;
    return ( g->rng_state >> 16 ) & 0x7FFF;
}

float frand( G* g )
{
    return ( rng( g ) % 1000 ) * 0.001;
}
