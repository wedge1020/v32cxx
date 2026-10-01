// *****************************************************************************
//  tests/97sample.cpp — ternary lowering, switch bodies, float literals
//
//  Regression test for three bugs found building the TEMPEST 32K demo:
//
//   1. A `?:` could reach the generated C unrewritten (the Vircon32 C lexer
//      then dies on '?'): inside switch case bodies, else-if conditions,
//      for-loop clauses, brace-less bodies, labeled statements, array
//      initializers, and a ternary used as a ternary's condition. Ternaries
//      must also keep C++'s evaluation rules: only the chosen branch runs,
//      the right side of && / || runs only when needed, and a loop
//      condition is re-evaluated on every pass (break/continue intact).
//      Laziness is checked with a call counter (bump).
//   2. Function calls inside switch case bodies were never name-mangled
//      (`f(2)` instead of `f__int(2)`) -- every lowering walker skipped
//      AST_SWITCH.
//   3. `t / 34.0` was emitted as `t / 34` -- integer division -- because
//      whole-number float literals lost their decimal point.
//   4. (20261001 later) the comma operator -- `a, b` -- which Vircon32 C
//      lacks, in expression statements, parentheses and for clauses
//      (continue still runs `i++, j--`); `for (int i = 0, j = 5; ...)`;
//      and a ternary mixing an enum with an int (the temporary must be an
//      int: Vircon32 C won't store an int into an enum variable).
// *****************************************************************************

#include "video.h"
#include "string.h"

int calls = 0;

int bump( int v )
{
    calls++;
    return v;
}

struct Box { int value; };

int ternary_laziness()
{
    int errors = 0;
    int yes = 1;
    int no = 0;

    calls = 0;
    int v = yes ? 7 : bump( 8 );                 // untaken branch: no call
    if( v != 7 || calls != 0 )                   errors++;

    calls = 0;
    int w = no ? bump( 1 ) : ( yes ? 2 : bump( 3 ) );
    if( w != 2 || calls != 0 )                   errors++;

    calls = 0;                                   // three levels, as a call argument
    int x = bump( no ? 2 : ( no ? 3 : ( yes ? 1 : 0 ) ) );
    if( x != 1 || calls != 1 )                   errors++;

    Box* box = 0;                                // the classic null guard
    int guarded = box ? box->value : -1;
    if( guarded != -1 )                          errors++;

    calls = 0;                                   // && must not evaluate its right side
    int both = no && ( bump( 1 ) ? 1 : 0 );
    if( both != 0 || calls != 0 )                errors++;

    calls = 0;                                   // || likewise
    int either = yes || ( bump( 1 ) ? 1 : 0 );
    if( either != 1 || calls != 0 )              errors++;

    int frame = 3;                               // parenthesized & condition
    int y = ( frame & 1 ) ? 5 : 4;
    if( y != 5 )                                 errors++;

    int z = ( yes ? no : yes ) ? 10 : 20;        // ternary as the condition
    if( z != 20 )                                errors++;

    float mixed = no ? 1 : 2.5;                  // int vs float branch: float
    if( mixed < 2.4 || mixed > 2.6 )             errors++;

    int pair[ 2 ] = { yes ? 4 : 5, 6 };
    if( pair[ 0 ] != 4 )                         errors++;

    return errors;
}

int ternary_statements()
{
    int errors = 0;
    int yes = 1;
    int no = 0;
    int r = 0;

    if( no ) r = 1;                              // else-if with a ternary condition
    else if( yes ? 1 : 0 ) r = 2;
    if( r != 2 )                                 errors++;

    if( yes ) r = bump( no ? 10 : 20 );          // brace-less body
    if( r != 20 )                                errors++;

    int total = 0;
here:
    total += yes ? 3 : 4;                        // labeled statement
    if( total < 6 ) goto here;
    if( total != 6 )                             errors++;

    return errors;
}

int ternary_loops()
{
    int errors = 0;
    int big = 1;

    // while: the condition is re-evaluated every pass
    int i = 0;
    while( i < ( big ? 5 : 2 ) )
    {
        i++;
        if( i == 3 ) big = 0;                    // limit drops to 2 -> loop ends
    }
    if( i != 3 )                                 errors++;

    // do-while: body runs first; continue still reaches the test
    int n = 0;
    int passes = 0;
    do
    {
        passes++;
        n++;
        if( n == 2 ) continue;
    } while( n < ( big ? 10 : 4 ) );
    if( n != 4 || passes != 4 )                  errors++;

    // for: ternary init, condition and increment; continue runs the increment
    int sum = 0;
    int visits = 0;
    for( int k = big ? 100 : 0; k < ( big ? 0 : 10 ); k += ( k < 4 ? 1 : 2 ) )
    {
        visits++;
        if( k == 1 ) continue;
        sum += k;
    }
    // k: 0 1 2 3 4 6 8 -> visits 7, sum 0+2+3+4+6+8 = 23
    if( visits != 7 || sum != 23 )               errors++;

    // brace-less loop bodies
    int acc = 0;
    for( int k = 0; k < 3; k++ ) acc += big ? 1 : 10;
    if( acc != 30 )                              errors++;

    return errors;
}

int switch_bodies()
{
    int errors = 0;

    for( int which = 0; which < 3; which++ )
    {
        int got = 0;
        switch( which > 1 ? 2 : which )           // ternary discriminant
        {
            case 0:
                got = bump( which ? 9 : 1 );      // call + ternary in a case
                break;
            case 1:
                got = bump( 2 );                  // plain call in a case
                break;
            default:
                got = which == 2 ? bump( 3 ) : 0;
        }
        if( got != which + 1 )                   errors++;
    }

    return errors;
}

int float_literals()
{
    int errors = 0;
    int t = 17;

    float a = t / 34.0;                          // 0.5, not integer 0
    if( a < 0.49 || a > 0.51 )                   errors++;

    float b = 1.0 - t / 34.0;                    // 0.5, not 1
    if( b < 0.49 || b > 0.51 )                   errors++;

    float c = 0.00001 * 100000;                  // no exponent form in the C
    if( c < 0.99 || c > 1.01 )                   errors++;

    return errors;
}

enum Blend { BLEND_A = 32, BLEND_B = 33 };

int comma_checks()
{
    int errors = 0;

    int i; int j;
    int s = 0;
    int passes = 0;
    for( i = 0, j = 5; i < j; i++, j-- )         // pairs (0,5) (1,4) (2,3)
    {
        passes++;
        if( i == 1 ) continue;                   // must still run i++, j--
        s += i * 10 + j;
    }
    if( passes != 3 || s != 5 + 23 )             errors++;

    int t = 0;
    for( int a = 0, b = 3; a < b; a++ ) t += a + b;   // 3 + 4 + 5
    if( t != 12 )                                errors++;

    int x = 0, y = 0;
    x = 1, y = 2;
    if( x != 1 || y != 2 )                       errors++;

    calls = 0;
    int z = ( x++, bump( y + 3 ) );              // x++ runs, z is the bump
    if( x != 2 || z != 5 || calls != 1 )         errors++;

    int n = 0;
    while( ( x += 1, x < 6 ) ) n++;              // x: 3 4 5 -> 6 stops
    if( n != 3 )                                 errors++;

    int on = 1;
    int mode = on ? BLEND_B : 0;                 // enum meets int: an int
    if( mode != 33 )                             errors++;

    return errors;
}

// -----------------------------------------------------------------------------

int test_errors = -1;

void main()
{
    int errors = ternary_laziness() + ternary_statements() + ternary_loops()
               + switch_bodies() + float_literals() + comma_checks();
    test_errors = errors;

    int text[ 32 ];
    itoa( errors, text, 10 );
    clear_screen( color_black );
    print_at( 20, 20, "errors:" );
    print_at( 100, 20, text );
}
