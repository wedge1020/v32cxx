// *****************************************************************************
//  tests/95sample.cpp — the C++-side preprocessor: #define, #undef, #if
//
//  v32c++ now expands macros itself (before parsing) instead of only
//  carrying #define lines through to the generated C. Deliberately
//  exercised, each checked at run time:
//   1. object-like constants as array sizes (`int a[MAX]`), arithmetic on
//      them (`[ROWS * COLS]`), enum constants as sizes, multi-dimensional
//      arrays, an array parameter, an array of function pointers;
//   2. float #defines typed as float (3 * HALF == 1.5, not 0);
//   3. negative constants (`x - LOW` must not become the decrement `x--`);
//   4. function-like macros: nesting, an invocation spread over several
//      lines, # stringizing, ## pasting, variadic __VA_ARGS__;
//   5. #ifdef / #ifndef / #if / #elif / #else, defined(), arithmetic in
//      conditions, a skipped region full of invalid code;
//   6. #undef + redefinition, and a self-referencing macro;
//   7. backslash-continued #define, comments inside definitions;
//   8. __LINE__, __V32CXX__, and -D from the command line (make test
//      passes -D FROM_CMDLINE=7).
//  Names of stable constants survive into the generated C (`int [MAX]`),
//  and their #defines are passed through ahead of the code.
// *****************************************************************************

#include "video.h"
#include "string.h"

#define MAX       5               // trailing comment must not leak into uses
#define ROWS      3
#define COLS      4
#define HALF      0.5
#define LOW       -3
#define NOPS      2
#define LETTER    'Q'
#define EMPTY
#define SQUARE(x)        ((x) * (x))
#define TWICE(x)         (2 * (x))
#define QUAD(x)          TWICE(TWICE(x))
#define STR(x)           #x
#define CAT(a, b)        a ## b
#define SUM3(...)        sum3(__VA_ARGS__)
#define LONG_SUM(a, b, c) \
        ((a) +            \
         (b) + (c))       /* a block comment in a continued define */

enum Tiles { TILE_EMPTY, TILE_WALL, TILE_DOOR, TILE_COUNT };

int sum3( int a, int b, int c )
{
    return a + b + c;
}

int add_one( int v ) { return v + 1; }
int add_two( int v ) { return v + 2; }

// an array parameter sized by a macro (decays to a pointer, as in C++)
int first_of( int values[ MAX ] )
{
    return values[ 0 ];
}

int array_checks()
{
    int errors = 0;

    int a[ MAX ] = { 1, 2, 3, 4, 5 };
    int grid[ ROWS ][ COLS ];
    int flat[ ROWS * COLS ];
    int by_enum[ TILE_COUNT ];
    int (*ops[ NOPS ])( int );

    if( sizeof( a ) != MAX )             errors++;
    if( sizeof( flat ) != 12 )           errors++;
    if( sizeof( by_enum ) != 3 )         errors++;
    if( sizeof( grid ) != ROWS * COLS )  errors++;

    for( int r = 0; r < ROWS; r++ )
        for( int c = 0; c < COLS; c++ )
            grid[ r ][ c ] = r * COLS + c;
    if( grid[ ROWS - 1 ][ COLS - 1 ] != 11 ) errors++;

    if( first_of( a ) != 1 )             errors++;
    if( a[ MAX - 1 ] != 5 )              errors++;

    ops[ 0 ] = add_one;
    ops[ 1 ] = add_two;
    if( ops[ NOPS - 1 ]( 10 ) != 12 )    errors++;

    return errors;
}

int constant_checks()
{
    int errors = 0;

    float f = 3 * HALF;                  // float literal, so 1.5
    if( f < 1.4 || f > 1.6 )             errors++;

    int x = 10;
    if( x - LOW != 13 )                  errors++;   // not x-- 3
    if( -LOW != 3 )                      errors++;

    int c = LETTER;
    if( c != 81 )                        errors++;

    int e = 1 EMPTY;
    if( e != 1 )                         errors++;

    return errors;
}

int function_macro_checks()
{
    int errors = 0;

    if( SQUARE( 3 + 1 ) != 16 )          errors++;
    if( QUAD( MAX ) != 20 )              errors++;
    if( TWICE( SQUARE( 2 ) ) != 8 )      errors++;

    int spread = LONG_SUM( 1,
                           2,
                           3 );
    if( spread != 6 )                    errors++;

    int total = SUM3( 1, 2,
                      3 );
    if( total != 6 )                     errors++;

    int CAT( count, 1 ) = 41;
    count1++;
    if( count1 != 42 )                   errors++;

    char word[ 8 ] = STR( hey );
    if( word[ 0 ] != 'h' || word[ 2 ] != 'y' || word[ 3 ] != 0 ) errors++;

    return errors;
}

int conditional_checks()
{
    int errors = 0;
    int hits = 0;

#ifdef MAX
    hits++;
#else
    errors++;
#endif

#ifndef NOT_DEFINED_ANYWHERE
    hits++;
#endif

#if MAX * 2 == 10 && defined( ROWS ) && !defined NOT_DEFINED_ANYWHERE
    hits++;
#elif 1
    errors++;
#endif

#if 0
    this line is not C++ at all and must never reach the parser ( ;
#elif COLS > 10
    errors++;
#else
    hits++;
#endif

#if defined( __V32CXX__ )
    hits++;
#endif

#ifdef FROM_CMDLINE
    if( FROM_CMDLINE != 7 ) errors++;
    hits++;
#endif

    if( hits != 6 )                      errors++;
    return errors;
}

int limit = 3;                           // the global the macro below wraps
#define limit (limit + 1)                // self-reference: expands ONCE

int undef_checks()
{
    int errors = 0;

    if( limit != 4 )                     errors++;
#undef limit
    if( limit != 3 )                     errors++;

#define STEP 2
    int s1 = STEP;
#undef STEP
#define STEP 5
    int s2 = STEP;
    if( s1 != 2 || s2 != 5 )             errors++;

    if( __LINE__ != 198 )                errors++;   // this exact source line

    return errors;
}

// -----------------------------------------------------------------------------

int test_errors = -1;

void main()
{
    int errors = array_checks() + constant_checks() + function_macro_checks()
               + conditional_checks() + undef_checks();
    test_errors = errors;

    int text[ 32 ];
    itoa( errors, text, 10 );
    clear_screen( color_black );
    print_at( 20, 20, "errors:" );
    print_at( 100, 20, text );
}
