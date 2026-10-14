// *****************************************************************************
//  tests/130sample.cpp — v32::String::append_float and the SDK's ftoa()
//
//  append_float() is ftoa() with a guard. Checked here:
//    1  the rounding carry (the SDK's ftoa() once wrote 0.999999 as "0.1";
//       fixed in string.h, 2026) -- this sample needs the fixed string.h
//       (c_api/string.h; tools/vircon32/build-tools.sh installs it) to pass
//       under the real toolchain
//    2  ordinary values: decimals, sign, leading decimal zeroes, no
//       trailing zeroes
//    4  values outside the int range, which ftoa() cannot format (it never
//       returns): "overflow" / "-overflow"
//  Self-checking: test_errors must end at 0.
// *****************************************************************************
#include "v32/string.hpp"

int test_errors = -1;

bool is( v32::String& s, int* text )
{
    return strcmp( s.c_str(), text ) == 0;
}

int carry()
{
    v32::String a;
    a.append_float( 0.999999 );
    v32::String b;
    b.append_float( 1.999999 );
    v32::String c;
    c.append_float( 0.5 );
    if( !is( a, "1" ) || !is( b, "2" ) || !is( c, "0.5" ) )
      return 1;
    return 0;
}

int ordinary()
{
    v32::String a;
    a.append_float( 2.5 );
    v32::String b;
    b.append_float( -0.125 );
    v32::String c;
    c.append_float( 3.0 );
    v32::String d;
    d.append_float( 0.00005 );
    v32::String e;
    e.append_float( 123.456 );
    v32::String f;
    f.append_float( 0.0 );
    if( !is( a, "2.5" ) || !is( b, "-0.125" ) || !is( c, "3" ) )
      return 2;
    if( !is( d, "0.00005" ) || !is( e, "123.456" ) || !is( f, "0" ) )
      return 2;
    return 0;
}

int range()
{
    v32::String a( "x=" );
    a.append_float( 2147483648.0 );
    v32::String b( "x=" );
    b.append_float( -2147483648.0 );
    v32::String c( "x=" );
    c.append_float( 4000000000.0 );
    if( !is( a, "x=overflow" ) || !is( b, "x=-overflow" ) || !is( c, "x=overflow" ) )
      return 4;
    return 0;
}

int main( void )
{
    int errors = 0;
    errors += carry();
    errors += ordinary();
    errors += range();
    test_errors = errors;
    return 0;
}
