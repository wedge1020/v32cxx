// *****************************************************************************
//  tests/133sample.cpp -- plain source conveniences, on the real toolchain
//
//  Things the Vircon32 C compiler itself lacks (or that its quirks list once
//  said were unsupported), and that C++ input may use freely:
//    // comments (also with ? : " ' and a trailing backslash inside them),
//    adjacent string literals across lines and around a comment,
//    a backslash-newline inside a literal and inside a macro, and the
//    ternary operator in every position.
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

#include "string.h"

#define SUM3(a, b, c) \
    ((a) + \
     (b) + \
     (c))

int test_errors = -1;

const char *global_msg = "abc" "def"      // two literals, one line
                         "ghi";            // and a third on the next

int pick( int a, int b )
{
    return a > b ? a : b;                  // ternary in a return
}

int main()
{
    int e = 0;
    int x = 3;     // comment with ? : and "quotes" and 'apostrophes'
    int y = x > 2 ? 10 : 20;               // ternary initializer
    int z = 0;
    char s[] = "one " "two" /* mid */ " three";
    const char *t = "long \
line";
    int  a[ 3 ];

    // a comment that ends with a backslash \
    x = 99;   // this line is still part of that comment: x stays 3

    /* a block
       comment */

    z = x == 3 ? (y == 10 ? 1 : 2) : 3;    // nested
    a[ x > 2 ? 0 : 1 ] = x < 2 ? 7 : 8;    // ternary in subscript and rhs

    if( x != 3 ) e++;
    if( y != 10 ) e++;
    if( z != 1 ) e++;
    if( a[ 0 ] != 8 ) e++;
    if( strlen( s ) != 13 ) e++;
    if( strlen( t ) != 9 ) e++;
    if( strlen( global_msg ) != 9 ) e++;
    if( SUM3( 1, 2, 3 ) != 6 ) e++;
    if( pick( 4, 9 ) != 9 ) e++;
    if( pick( 5, 2 + (x > 2 ? 1 : 0) ) != 5 ) e++;

    test_errors = e;
    return 0;
}
