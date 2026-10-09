// *****************************************************************************
//  tests/119sample.cpp — v32/keyboard.hpp and v32/mouse.hpp, driven
//  by scripted v32io input
//
//  A keyboard on gamepad port 1 and a mouse on port 2, fed by
//  tests/119sample.input -- a v32peek input script encoding real v32kbd and
//  v32mouse protocol traffic, generated from tests/119sample.actions by
//  tools/vircon32/v32io-script.py. check.sh notices the .input file and
//  runs this cartridge with it (every other self-checking sample runs with
//  no input at all).
//
//   phase 1  every key event through next_event(): key codes, typed
//            symbols (shift, caps lock), press/release, is_down/ctrl_down
//   phase 2  after F1: presses through read() (releases skipped)
//   mouse    pointer position at each button press, total movement in
//            steps, pointer clamped at the screen edge, button edges
//   phase 3  after Escape: nobody reads; 72 events overflow the 64-event
//            queue, which must keep the oldest 64 and count 8 dropped
//
//  Self-checking: test_errors must end at 0.
// *****************************************************************************

#include <v32/keyboard.hpp>
#include <v32/mouse.hpp>

int test_errors = -1;

// phase 1: the events next_event() must hand out, in order
int expect_key[ 28 ]     = { 6, 'h', 'h', 6, 'i', 'i', 6, '1', '1', 6,
                             5, 5, 'a', 'a', 6, 'b', 'b', 6, 5, 5,
                             'l', 'l', 'l', 'l', 10, 'c', 'c', 10 };
int expect_symbol[ 28 ]  = { 6, 'H', 'H', 6, 'i', 'i', 6, '!', '!', 6,
                             5, 5, 'A', 'A', 6, 'b', 'b', 6, 5, 5,
                             'l', 'l', 'l', 'l', 10, 'c', 'c', 10 };
int expect_pressed[ 28 ] = { 1, 1, 0, 0, 1, 0, 1, 1, 0, 0,
                             1, 0, 1, 0, 1, 1, 0, 0, 1, 0,
                             1, 0, 1, 0, 1, 1, 0, 0 };

// phase 2: what read() must return
int expect_read[ 4 ]     = { 'x', v32::KeyEnter, v32::KeyF12, v32::KeyEscape };

v32::Keyboard keyboard( v32::SecondGamepadPort );

int main()
{
    v32::Mouse    mouse( v32::ThirdGamepadPort );
    v32::KeyEvent event;
    int  e          = 0;
    int  phase      = 1;
    int  seen       = 0;
    int  reads      = 0;
    int  phase3_from = 0;
    int  key        = 0;
    int  steps_x    = 0;
    int  steps_y    = 0;
    int  left_down  = 0;
    int  left_up    = 0;
    int  right_down = 0;
    int  left_x     = -1;
    int  left_y     = -1;
    int  right_x    = -1;
    int  right_y    = -1;
    bool pressed    = false;
    bool ctrl_at_c  = false;

    if( keyboard.port() != 1 || mouse.port() != 2 ) e++;
    if( mouse.x() != 320 || mouse.y() != 180 || mouse.scale() != 2 ) e++;

    while( phase < 4 )
    {
        keyboard.probe();
        mouse.probe();
        keyboard.probe();             // a second probe in one frame: no-op

        // ---- keyboard -------------------------------------------------------
        if( phase == 1 )
        {
            while( phase == 1 && keyboard.next_event( &event ) )
            {
                if( event.key == v32::KeyF1 )
                {
                    if( seen != 28 ) e++;
                    phase = 2;
                }
                else if( seen < 28 )
                {
                    if( event.key != expect_key[ seen ] ) e++;
                    if( event.symbol != expect_symbol[ seen ] ) e++;
                    if( (event.pressed ? 1 : 0) != expect_pressed[ seen ] ) e++;
                    if( event.key == 'c' && event.pressed )
                      ctrl_at_c = keyboard.ctrl_down() && keyboard.is_down( v32::KeyLCtrl );
                    seen++;
                }
                else
                  e++;
            }
        }
        else if( phase == 2 )
        {
            key = keyboard.read();
            while( phase == 2 && key > 0 )
            {
                if( reads < 4 && key != expect_read[ reads ] ) e++;
                reads++;
                if( key == v32::KeyEscape )
                {
                    phase       = 3;
                    phase3_from = get_frame_counter();
                }
                else
                  key = keyboard.read();
            }
        }
        else if( phase == 3 )
        {
            // Escape's release arrives on the next frame: drop it, so the
            // queue holds only the burst (which starts 3 frames later)
            if( get_frame_counter() == phase3_from + 2 )
              keyboard.flush();

            // wait for the burst to end: 30 quiet frames after an event
            if( keyboard.pending() > 0 &&
                get_frame_counter() - keyboard.last_event() > 30 )
              phase = 4;
        }

        // ---- mouse ----------------------------------------------------------
        steps_x += mouse.steps_x();
        steps_y += mouse.steps_y();
        if( mouse.steps_x() * mouse.scale() != mouse.dx() ) e++;
        if( mouse.pressed( v32::MouseLeft ) )
        {
            left_down++;
            mouse.position( &left_x, &left_y );
        }
        if( mouse.released( v32::MouseLeft ) ) left_up++;
        if( mouse.pressed( v32::MouseRight ) )
        {
            right_down++;
            right_x = mouse.x();
            right_y = mouse.y();
            if( !mouse.is_down( v32::MouseRight ) || mouse.is_down( v32::MouseLeft ) ) e++;
        }

        end_frame();
    }

    // keyboard results
    if( reads != 4 ) e++;
    if( !ctrl_at_c ) e++;
    if( keyboard.caps_lock_on() || keyboard.shift_down() || keyboard.ctrl_down() ) e++;
    if( keyboard.pending() != 64 || keyboard.dropped() != 8 ) e++;
    key = keyboard.read_event( &pressed );         // the oldest kept: 'q' down
    if( key != 'q' || !pressed ) e++;
    key = keyboard.read_event( NULL );             // then 'q' up
    if( key != 'q' ) e++;
    keyboard.flush();
    if( keyboard.pending() != 0 || keyboard.read() != 0 ) e++;
    if( !keyboard.connected() ) e++;

    // mouse results: 320 + (50 - 12) * 2 = 396, 180 + (20 - 10) * 2 = 200
    if( left_down != 1 || left_up != 1 || right_down != 1 ) e++;
    if( left_x != 396 || left_y != 200 ) e++;
    if( right_x != 402 || right_y != 200 ) e++;
    if( steps_x != 441 || steps_y != 10 ) e++;
    if( mouse.x() != 639 || mouse.y() != 200 ) e++;     // clamped
    if( mouse.buttons() != 0 || !mouse.connected() ) e++;

    // the decoding helpers, on their own
    if( v32::mouse_counter( 0, v32::MouseXNegative, v32::MouseXPositive, v32::MouseXHigh, v32::MouseXLow ) != 1 ) e++;
    if( v32::mouse_counter( v32::MouseXNegative | v32::MouseXPositive, v32::MouseXNegative,
                            v32::MouseXPositive, v32::MouseXHigh, v32::MouseXLow ) != -1 ) e++;
    if( v32::mouse_steps( 11, 2 ) != 3 || v32::mouse_steps( 2, 11 ) != -3 ) e++;
    if( v32::mouse_steps( 0, 6 ) != 0 || v32::mouse_steps( -1, 4 ) != 0 ) e++;

    // bounds and placement
    mouse.set_bounds( 10, 20, 100, 200 );
    if( mouse.x() != 100 || mouse.y() != 200 ) e++;
    mouse.set_position( -5, 50 );
    if( mouse.x() != 10 || mouse.y() != 50 ) e++;
    mouse.set_scale( 0 );
    if( mouse.scale() != 2 ) e++;

    // the core underneath
    if( keyboard.device()->port() != 1 ) e++;
    if( (mouse.device()->read() & v32::IoAll) != mouse.device()->read() ) e++;

    test_errors = e;
    return 0;
}
