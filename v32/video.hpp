#pragma once
// *****************************************************************************
//  v32/video.hpp — pilot C++ veneer over Vircon32's "video.h"
//
//  Consumption model: this file is C++ source in the v32c++ subset, meant to
//  be inlined into the user's translation unit by v32pp (or included once a
//  real preprocessor exists inside the transpiler). The "#include" below is
//  a pass-through line: v32c++ re-emits it verbatim, and the downstream
//  Vircon32 C compiler resolves the real video.h that provides every
//  function called here. Nothing in this file re-implements hardware
//  access — it only wraps the C API in the C++ subset the transpiler
//  supports (no templates, no exceptions, no ternary, one-word params).
// *****************************************************************************

#include "video.h"

namespace v32
{
    // -------------------------------------------------------------------------
    //  Typed constants for the blending modes (the C API uses #defines)
    // -------------------------------------------------------------------------

    enum BlendMode
    {
        BlendAlpha    = 0x20,
        BlendAdd      = 0x21,
        BlendSubtract = 0x22
    };

    // -------------------------------------------------------------------------
    //  Convenience overloads the C API cannot express
    //
    //  These deliberately get names that differ from the C originals so the
    //  mangled names can never be confused with a pass-through C call: users
    //  can still call plain clear_screen()/print_at() from v32c++ source and
    //  those resolve against the C header downstream, untouched.
    // -------------------------------------------------------------------------

    // clear by separate RGB components instead of a packed color word
    void clear_screen_rgb( int r, int g, int b )
    {
        clear_screen( make_color_rgb( r, g, b ) );
    }

    // version of print_at with the C-style parameter order kept
    void print_xy( int x, int y, int* text )
    {
        print_at( x, y, text );
    }

    // draw a region in one call, hiding the select/draw two-step;
    // applies to the currently selected texture
    void draw( int region_id, int x, int y )
    {
        select_region( region_id );
        draw_region_at( x, y );
    }

    // same, but placing the region's hotspot at the given point
    void draw_scaled( int region_id, int x, int y, float scale )
    {
        select_region( region_id );
        set_drawing_scale( scale, scale );
        draw_region_zoomed_at( x, y );
    }

    // set blending through the typed enum instead of a raw hex constant
    void set_blending( BlendMode mode )
    {
        set_blending_mode( mode );
    }

    // -------------------------------------------------------------------------
    //  RAII scope guard for the GPU's implicit selected-texture state
    //
    //  This is the classic footgun print_at itself hand-rolls (it saves and
    //  restores the previous texture). With phase-9 destructor invocation,
    //  the transpiler turns this into the same save/restore pattern at the
    //  closing brace — on both the fall-off-the-end path and an early
    //  return. No exceptions exist in this language, so this is complete
    //  RAII for the subset.
    //
    //  NOTE: the class itself is one pointer-sized state word and all its
    //  members are one-word types, so it passes by value fine — but we
    //  still avoid relying on that: guards are meant to be stack locals.
    // -------------------------------------------------------------------------

    class TextureScope
    {
        int previous_texture;

        public:
            TextureScope( int texture_id )
            {
                previous_texture = get_selected_texture();
                select_texture( texture_id );
            }

            ~TextureScope()
            {
                select_texture( previous_texture );
            }
    };

    // usage:
    //
    //     {
    //         v32::TextureScope scope( 3 );
    //         v32::draw( 0, 100, 100 );
    //     }   // selection restored here, even on an early return
    //
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    //  A tiny 2D point helper — one-word members only, and every method
    //  takes and returns it BY REFERENCE or by primitive, never by value
    //  (Vircon32 C requires one-word params/returns; this is the pattern the
    //  by-value lint rules are meant to enforce across all v32 headers)
    // -------------------------------------------------------------------------

    class Point
    {
        public:
            int x;
            int y;

            Point()
            {
                x = 0;
                y = 0;
            }

            Point( int px, int py )
            {
                x = px;
                y = py;
            }

            // mutating style: no by-value return, no allocation
            void add( const Point& other )
            {
                x += other.x;
                y += other.y;
            }
    };

    // out-param style instead of a by-value Point return
    void screen_center( Point* out )
    {
        out->x = screen_width / 2;
        out->y = screen_height / 2;
    }
}
