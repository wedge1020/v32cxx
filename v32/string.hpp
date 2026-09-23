#pragma once
// *****************************************************************************
//  v32/string.hpp — pilot C++ layer over Vircon32's "string.h"
//
//  Same consumption model as v32/video.hpp: inlined by v32pp into the
//  user's translation unit; the "#include" lines below are pass-through and
//  resolved by the downstream Vircon32 C compiler.
//
//  Design notes for the one-word param/return constraint:
//   - All methods return void, int, or int* — never a String by value.
//   - Concatenation is mutating (append/operator+= style), never
//     "operator+ returns a new String" (that would need a by-value or
//     heap return; heap return is possible but shifts ownership to the
//     caller, so it is avoided in the pilot).
//   - The buffer is a fixed in-object array, so no malloc/free and no
//     destructor needed — String is safely copyable word-by-word, which
//     matters because Vircon32 struct assignment copies shallowly.
// *****************************************************************************

#include "string.h"
#include "video.h"   // only needed if print() below is used; harmless otherwise

namespace v32
{
    // -------------------------------------------------------------------------
    //  Fixed-capacity string over Vircon32's int* null-terminated strings
    // -------------------------------------------------------------------------

    class String
    {
        int buffer[ 128 ];

        public:

            String()
            {
                buffer[ 0 ] = 0;
            }

            // construct from a literal or any int* string
            String( int* text )
            {
                strcpy( buffer, text );
            }

            // length in characters (excluding the null terminator)
            int length()
            {
                return strlen( buffer );
            }

            // access to the underlying int* string, e.g. for the C API
            int* c_str()
            {
                return buffer;
            }

            // character access with bounds clamp (no ternary in the subset)
            int at( int index )
            {
                if( index < 0 )
                  return 0;
                if( index >= length() )
                  return 0;
                return buffer[ index ];
            }

            // ---- mutation ---------------------------------------------------

            void assign( int* text )
            {
                strcpy( buffer, text );
            }

            void append( int* text )
            {
                strcat( buffer, text );
            }

            // mutating operator: this is the pattern that replaces a
            // by-value operator+ under the one-word constraint
            void operator+=( int* text )
            {
                strcat( buffer, text );
            }

            // whole-string replacement; the natural spelling of assign()
            void operator=( int* text )
            {
                strcpy( buffer, text );
            }

            // append a formatted integer (bases 2-16, base 10 signed)
            void append_int( int value, int base )
            {
                int digits[ 32 ];
                itoa( value, digits, base );
                strcat( buffer, digits );
            }

            void append_float( float value )
            {
                int digits[ 32 ];
                ftoa( value, digits );
                strcat( buffer, digits );
            }

            // ---- comparison -------------------------------------------------

            bool equals( int* text )
            {
                return (strcmp( buffer, text ) == 0);
            }

            bool operator==( int* text )
            {
                return (strcmp( buffer, text ) == 0);
            }

            bool operator!=( int* text )
            {
                return (strcmp( buffer, text ) != 0);
            }

            // ---- printing ----------------------------------------------------

            // convenience: print at a position using the bios font
            void print_at_xy( int x, int y )
            {
                print_at( x, y, buffer );
            }

            // convenience: print at the current drawing point;
            // spelled out via print_at rather than calling the C API's
            // own print() — an unqualified print( buffer ) inside this
            // class could collide with this very method in name lookup
            void print_current_point()
            {
                int x, y;
                get_drawing_point( &x, &y );
                print_at( x, y, buffer );
            }
    };

    // -------------------------------------------------------------------------
    //  usage example (subset-safe):
    //
    //      v32::String s( "Score: " );
    //      s.append_int( player_score, 10 );
    //      s += " points";
    //      s.print_at_xy( 20, 20 );
    //
    //  note: string literals are int-array initializers in this transpiler,
    //  so "Score: " is a valid argument for an int* parameter
    // -------------------------------------------------------------------------
}
