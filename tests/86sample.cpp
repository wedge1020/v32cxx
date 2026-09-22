/* 86sample.cpp -- inline assembly tests for v32c++.
 *
 * Exercises every asm spelling the transpiler accepts, plus one
 * case that must produce the targeted extended-asm diagnostic.
 * Round-trips several functions verbatim from video.h (the GPU
 * register names and GPUCommand_* constants live in the BIOS, so
 * this compiles as-is against the real cartridge toolchain when
 * included alongside <video.h>).
 */

#include "video.h"

/* ---- 1. Vircon32 native brace form, multiple literals ---------------- *
 * The most common shape: every wrapper in video.h looks like this.
 * Pass-through must preserve the {param} interpolation verbatim,
 * including a parameter that this-injection would normally reach
 * (it must NOT -- the interpolation is resolved by Vircon32's C
 * compiler, not by us). Prefixed my_ so these don't collide with
 * video.h's own definitions of the same functions -- the asm bodies
 * are copied verbatim from the header, only the C names differ. */
void my_select_texture( int texture_id )
{
    asm
    {
        "mov R0, {texture_id}"
        "out GPU_SelectedTexture, R0"
    }
}

/* ---- 2. Brace form with a value returned via R0 ---------------------- *
 * Statement-level asm leaving its result in R0: Vircon32 C treats
 * that as the function's return value, so no expression form is
 * needed. Single-literal body. */
int my_get_selected_texture()
{
    asm
    {
        "in R0, GPU_SelectedTexture"
    }
}

/* ---- 3. Brace form using a pointer-out idiom ------------------------- *
 * Copied from video.h's get_drawing_point: writes through a pointer
 * parameter from inside asm. Two literals, pointer arithmetic in
 * Vircon32 assembly syntax. */
void my_get_drawing_point( int* drawing_x, int* drawing_y )
{
    asm
    {
        "push R1"
        "in R0, GPU_DrawingPointX"
        "mov R1, {drawing_x}"
        "mov [R1], R0"
        "in R0, GPU_DrawingPointY"
        "mov R1, {drawing_y}"
        "mov [R1], R0"
    }
}

/* ---- 4. GCC/Clang basic asm spelling --------------------------------- *
 * Re-emitted as Vircon32 brace form. Adjacent-literal concatenation
 * is allowed in this form too (real C's own rule). */
void wait_frames( int frames )
{
    asm( "mov R0, {frames}" "wait_here: SUB R0, 1, R0" "JNZ wait_here" );
}

/* ---- 5. volatile qualifier + underscore spellings --------------------- *
 * The qualifier governs only optimization, which v32c++ performs
 * none of -- accepted and dropped. All keyword spellings collapse
 * to the same AST_ASM node. */
void vsync()
{
    __asm( "in R0, ScanlineCounter" );
    asm volatile( "wait_vbl: CMP R0, 359" "JZ wait_vbl" );
    __asm__( "nop" );
}

/* ---- 6. asm inside class methods (after this-injection) -------------- *
 * The {width} interpolation refers to an injected-this FIELD access.
 * This-injection must leave the literal untouched: hoisting to a
 * local (as done here with w) is the caller's job, since {this->w}
 * is not valid interpolation syntax. Also exercises asm inside a
 * loop body, so sema's loop-depth tracking and lower.c's phase-9
 * statement walk both pass over an AST_ASM. */
class Clipper
{
    int x;
    
public:
    
    Clipper() { x = 0; }
    
    void step_right()
    {
        int w = 8;
        
        for( int i = 0; i < 4; i++ )
        {
            asm
            {
                "mov R0, {w}"
                "iadd R0, 1"
                "out GPU_DrawingPointX, R0"
            }
        }
    }
};

/* ---- 7. Expected to FAIL with the targeted diagnostic ----------------- *
 * Uncomment to verify extended asm produces the clear "operand
 * constraints are not supported" error rather than a generic
 * "syntax error, unexpected ':'":
 *
 * void bad_extended( int x )
 * {
 *     asm( "mov %0, %1" : "=r"( x ) : "r"( x ) );
 * }
 * ------------------------------------------------------------------------ */

/* ---- 8. Sanity: asm mixed with ordinary statements -------------------- *
 * An asm block between ordinary code, including a destructible
 * local before it (phase 9 must append the destructor call AROUND
 * the asm statement, never inside or before it). */
class Noisy
{
public:
    Noisy()  { print( (int*)"ctor\n" ); }
    ~Noisy() { print( (int*)"dtor\n" ); }
};

void mixed()
{
    Noisy n;
    
    asm
    {
        "out GPU_Command, GPUCommand_DrawRegion"
    }
    
    my_select_texture( 0 );
}

/* ---- main: just enough to drive it ------------------------------------ *
 * Vircon32 requires void main(); the transpiler handles that rewrite.
 * print() comes from the BIOS via video.h's own machinery.
 */
void main()
{
    my_select_texture( 1 );
    set_drawing_point( 100, 100 );
    
    Clipper c;
    c.step_right();
    
    mixed();
    
    int x, y;
    my_get_drawing_point( &x, &y );
    
    print( (int*)"done\n" );
}
