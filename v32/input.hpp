#pragma once
// *****************************************************************************
//  v32/input.hpp — pilot C++ veneer over Vircon32's "input.h"
//
//  Consumption model: same as v32/video.hpp — this file is C++ source in the
//  v32c++ subset, inlined into the user's translation unit by v32pp (or the
//  transpiler's own include expansion). The "#include" below is a pass-through
//  line: v32c++ re-emits it verbatim, and the downstream Vircon32 C compiler
//  resolves the real input.h that provides every function called here. Nothing
//  here touches hardware registers — the C header's asm blocks do that.
//
//  Naming rule (inherited from video.hpp): every wrapper gets a name that
//  DIFFERS from the C original, so a mangled v32:: name can never be confused
//  with a pass-through call. Users who want the raw C API can still call
//  gamepad_left() etc. directly from v32c++ source and it resolves downstream.
//
//  Includes a Button enum with pressed(Button): the transpiler already
//  lowers switch/case (AST_SWITCH passes through to codegen unchanged
//  since Vircon32 C natively supports fall-through switch), so the
//  dispatch is a plain switch over the enum.
// *****************************************************************************

#include "input.h"

namespace v32
{
    // -------------------------------------------------------------------------
    //  Thin readers for the currently selected gamepad.
    //
    //  The C API returns int (frames held); we keep the bool "is it on" view
    //  because that is what control flow wants 99% of the time. The raw int
    //  stays available by calling the C functions directly.
    // -------------------------------------------------------------------------

    // yes/no for the currently selected gamepad; unlike directions and
    // buttons this is only a connected/not-connected value
    bool connected()
    {
        return gamepad_is_connected();
    }

    bool left()
    {
        return gamepad_left() > 0;
    }

    bool right()
    {
        return gamepad_right() > 0;
    }

    bool up()
    {
        return gamepad_up() > 0;
    }

    bool down()
    {
        return gamepad_down() > 0;
    }

    // -------------------------------------------------------------------------
    //  Buttons of the currently selected gamepad (bool view of the C readers)
    // -------------------------------------------------------------------------

    bool button_a()
    {
        return gamepad_button_a() > 0;
    }

    bool button_b()
    {
        return gamepad_button_b() > 0;
    }

    bool button_x()
    {
        return gamepad_button_x() > 0;
    }

    bool button_y()
    {
        return gamepad_button_y() > 0;
    }

    bool button_l()
    {
        return gamepad_button_l() > 0;
    }

    bool button_r()
    {
        return gamepad_button_r() > 0;
    }

    bool button_start()
    {
        return gamepad_button_start() > 0;
    }

    // -------------------------------------------------------------------------
    //  Typed button identity + switch-based dispatch over the C readers.
    //
    //  The C API has no button type -- just seven separate readers. This
    //  enum gives them a single identity so game code can hold "which
    //  button" in a variable. pressed() dispatches with a switch, which
    //  needs no special lowering: AST_SWITCH passes through to codegen and
    //  Vircon32 C has native fall-through switch. No default case: an
    //  out-of-range button is a programming error we let surface loudly
    //  rather than silently report "not pressed".
    // -------------------------------------------------------------------------

    enum Button
    {
        ButtonA     = 0,
        ButtonB     = 1,
        ButtonX     = 2,
        ButtonY     = 3,
        ButtonL     = 4,
        ButtonR     = 5,
        ButtonStart = 6
    };

    // frames-held view for when the caller needs timing, not just yes/no
    int button_frames_held( Button button )
    {
        switch( button )
        {
            case ButtonA:     return gamepad_button_a();
            case ButtonB:     return gamepad_button_b();
            case ButtonX:     return gamepad_button_x();
            case ButtonY:     return gamepad_button_y();
            case ButtonL:     return gamepad_button_l();
            case ButtonR:     return gamepad_button_r();
            case ButtonStart: return gamepad_button_start();
        }
        return 0;
    }

    // int-taking overload: Vircon32 enums ARE ints downstream, so a loop
    // iterating b = 0..6 should not need a cast to reach the dispatcher.
    // NOTE: this also works around a current transpiler limit -- a C-style
    // cast with a QUALIFIED target, (v32::Button)b, does not parse,
    // because the cast production only recognizes a bare TYPE_NAME token
    // after '(' (the lexer's feedback sees "v32" as an identifier, not a
    // type). Use this overload or the enum constants, never that cast.
    int button_frames_held( int button )
    {
        switch( button )
        {
            case ButtonA:     return gamepad_button_a();
            case ButtonB:     return gamepad_button_b();
            case ButtonX:     return gamepad_button_x();
            case ButtonY:     return gamepad_button_y();
            case ButtonL:     return gamepad_button_l();
            case ButtonR:     return gamepad_button_r();
            case ButtonStart: return gamepad_button_start();
        }
        return 0;
    }

    bool pressed( Button button )
    {
        return button_frames_held( button ) > 0;
    }

    // -------------------------------------------------------------------------
    //  D-pad direction as a Point, instead of the C API's two out-params.
    //
    //  Same out-param idiom as v32::screen_center(Point*): never return a
    //  struct by value. We reuse v32::Point from video.hpp — input.hpp does
    //  NOT re-declare it; include order is video.hpp before input.hpp (same
    //  one-definition situation the two headers already have via #pragma
    //  once in the real preprocessor; v32pp's canonical-path dedupe handles
    //  the double include in the meantime).
    //
    //  The C gamepad_direction() writes -1/0/1 into each axis, matching
    //  Point's int members exactly, so this is a pure forwarding wrapper.
    // -------------------------------------------------------------------------

    void direction( Point* out )
    {
        gamepad_direction( (&out->x), (&out->y) );
    }

    // normalized variant: diagonals come back as a unit vector
    // (each axis +-0.70710678 when both are pressed) — needs Point to grow
    // float members or a FloatPoint sibling, so it is NOT wrapped yet;
    // call gamepad_direction_normalized() directly for now.

    // -------------------------------------------------------------------------
    //  RAII scope guard for the SPU-style "currently selected gamepad" state.
    //
    //  Exactly the TextureScope pattern from video.hpp, applied to the
    //  input side: save the previously selected gamepad, select the one you
    //  want, and the closing brace (or an early return — phase 9 inserts the
    //  dtor on both paths) restores it. One int of state, all members
    //  one-word: meant to be a stack local.
    // -------------------------------------------------------------------------

    class GamepadScope
    {
        int previous_gamepad;

        public:
            GamepadScope( int gamepad_id )
            {
                previous_gamepad = get_selected_gamepad();
                select_gamepad( gamepad_id );
            }

            ~GamepadScope()
            {
                select_gamepad( previous_gamepad );
            }
    };

    // usage:
    //
    //     v32::Point move;
    //     {
    //         v32::GamepadScope pad( 0 );   // gamepad 0 selected
    //         v32::direction( (&move) );
    //         bool jump = v32::button_a();
    //     }                                 // previous selection restored
    //
    // -------------------------------------------------------------------------
}
