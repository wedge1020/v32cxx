#pragma once
// *****************************************************************************
//  v32/math.hpp — thin C++ veneer over Vircon32's "math.h"
//
//  Same consumption model as the other v32/ headers: inlined into the
//  user's translation unit by v32c++'s own include resolution (-I); the
//  "#include" below is a pass-through line, resolved by the downstream
//  Vircon32 C compiler. Every numeric operation here is a call into the
//  C header (whose functions are single CPU instructions: imin, fmin,
//  iabs, flr, sin, pow, ...); nothing here re-implements the math.
//
//  What this layer adds is what C cannot express: OVERLOAD SETS. The C
//  API needs a separate name per type (min/fmin, max/fmax, abs/fabs);
//  here one name covers both int and float, picked by argument type.
//
//  Naming rule (same as video.hpp/input.hpp): wrappers never reuse a C
//  function's exact name. That is not just style: inside namespace v32 an
//  unqualified call to min() would find v32::min first -- the wrapper
//  would call itself. Hence minimum/maximum/absolute, not min/max/abs.
//
//  Overload resolution and pass-through arguments: an argument whose type
//  v32c++ can't see -- a C API call such as rand(), or an SDK #define such
//  as screen_width -- is fine as long as ANOTHER argument's known type
//  settles which overload is meant:
//
//      v32::clamp( x, 0, screen_width )      // x is int: int overload
//      v32::minimum( rand(), 10 )            // 10 is int: int overload
//
//  When no argument's type is known (v32::absolute( rand() )) the call is
//  reported as unresolvable; store the value in a typed local first.
//
//  Constants: pi, INT_MIN and INT_MAX are the C header's #defines and are
//  usable directly from v32c++ source as pass-through identifiers.
// *****************************************************************************

#include "math.h"

namespace v32
{
    // -------------------------------------------------------------------------
    //  min / max / abs as one overload set per operation
    // -------------------------------------------------------------------------

    int minimum( int a, int b )
    {
        return min( a, b );
    }

    float minimum( float a, float b )
    {
        return fmin( a, b );
    }

    int maximum( int a, int b )
    {
        return max( a, b );
    }

    float maximum( float a, float b )
    {
        return fmax( a, b );
    }

    int absolute( int a )
    {
        return abs( a );
    }

    float absolute( float x )
    {
        return fabs( x );
    }

    // -------------------------------------------------------------------------
    //  clamp: keep a value inside [low, high]. Not in the C API at all;
    //  built from the same min/max instructions (max(low, min(v, high))).
    //  If low > high the result is low, matching that formula.
    // -------------------------------------------------------------------------

    int clamp( int value, int low, int high )
    {
        return max( low, min( value, high ) );
    }

    float clamp( float value, float low, float high )
    {
        return fmax( low, fmin( value, high ) );
    }

    // -1, 0 or +1 by the sign of the value
    int sign( int value )
    {
        if( value > 0 ) return 1;
        if( value < 0 ) return -1;
        return 0;
    }

    float sign( float value )
    {
        if( value > 0.0 ) return 1.0;
        if( value < 0.0 ) return -1.0;
        return 0.0;
    }

    // -------------------------------------------------------------------------
    //  Angles. The GPU's set_drawing_angle() and the trig functions all
    //  take radians; game code usually thinks in degrees.
    // -------------------------------------------------------------------------

    float to_radians( float degrees )
    {
        return degrees * (pi / 180.0);
    }

    float to_degrees( float radians )
    {
        return radians * (180.0 / pi);
    }

    // -------------------------------------------------------------------------
    //  Small float helpers for movement and animation
    // -------------------------------------------------------------------------

    // linear interpolation: t = 0 gives a, t = 1 gives b
    float lerp( float a, float b, float t )
    {
        return a + (b - a) * t;
    }

    // straight-line distance between two points. sqrt() is undefined for
    // negative input on Vircon32 (hardware error), but a sum of squares
    // never is negative, so this is always safe to call.
    float distance( float x1, float y1, float x2, float y2 )
    {
        float dx = x2 - x1;
        float dy = y2 - y1;
        return sqrt( dx * dx + dy * dy );
    }

    // squared distance: no sqrt, so cheaper -- enough for "is it closer
    // than r" checks (compare against r * r)
    float distance_squared( float x1, float y1, float x2, float y2 )
    {
        float dx = x2 - x1;
        float dy = y2 - y1;
        return dx * dx + dy * dy;
    }
}
