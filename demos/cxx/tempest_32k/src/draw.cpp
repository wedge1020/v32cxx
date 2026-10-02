// *****************************************************************************
//  TEMPEST 32K -- src/draw.cpp
//  colors and the draw primitives: glyphs, bars, text, the debug meters
// *****************************************************************************
#include "tempest.hpp"

// ---------------------------------------------------------------------------
//  Color helpers — colors are ints, ABGR word order (red = low byte)
// ---------------------------------------------------------------------------
int make_color( int r, int g_, int b )
{
    return ( 255 << 24 ) | ( b << 16 ) | ( g_ << 8 ) | r;
}

// make_color for CONSTANT components: as a macro the Vircon32 C
// compiler folds the whole expression to one immediate -- no call, no
// shifts at run time. (make_color stays for computed components.)
#define RGB( r, g_, b ) ( ( 255 << 24 ) | ( (b) << 16 ) | ( (g_) << 8 ) | (r) )

int hue( int h )
{
    int region = ( h / 43 ) % 6;
    int f = ( h % 43 ) * 6;
    if( region == 0 ) return make_color( 255, f, 0 );
    if( region == 1 ) return make_color( 255 - f, 255, 0 );
    if( region == 2 ) return make_color( 0, 255, f );
    if( region == 3 ) return make_color( 0, 255 - f, 255 );
    if( region == 4 ) return make_color( f, 0, 255 );
    return make_color( 255, 0, 255 - f );
}

// All "glow" drawing routes through here, gated per layer by the
// GLOW_* switches; GLOW_MODE picks the blending used by enabled
// layers: 0 = solid, 1 = Add, 2 = Alpha. (Alpha blending is the
// fallback glow if the emulator's Add path proves broken.)
void set_glow( int layer )
{
    int on = GLOW_ALL;
    if( layer == 0 ) { if( GLOW_BULLET ) on = 1; }
    if( layer == 1 ) { if( GLOW_PART )   on = 1; }
    if( layer == 2 ) { if( GLOW_SPIKE )  on = 1; }
    if( layer == 3 ) { if( GLOW_CLAW )   on = 1; }
    if( layer == 4 ) { if( GLOW_ENEMY )  on = 1; }
    set_blending_mode( on ? GLOW_BLEND : BLEND_SOLID );
}

// ---------------------------------------------------------------------------
//  Glyph renderer — the ONLY draw primitive in the whole game
// ---------------------------------------------------------------------------
// Draws BIOS font character c centered at (x,y), sized w*h pixels.
// Base glyph is 10x20 px, so scale factors are (w/10, h/20). Caches the
// last region + scale in g to skip redundant GPU register writes —
// a big deal at hundreds of glyphs per frame.
void draw_glyph( G* g, int c, float x, float y, float w, float h )
{
    if( g->last_region != c )
    {
        select_region( c );
        g->last_region = c;
    }
    float sx = w / 10.0;
    float sy = h / 20.0;
    if( g->last_scale_x != sx || g->last_scale_y != sy )
    {
        set_drawing_scale( sx, sy );
        g->last_scale_x = sx;
        g->last_scale_y = sy;
    }
    // ZOOMED command, not plain DrawRegion: the GPU command table
    // shows plain DrawRegion ignores the scale ports — only the
    // Zoomed/Rotozoomed variants apply them
    draw_region_zoomed_at( (int)( x - w / 2 ), (int)( y - h / 2 ) );
    // DEBUG GPU METER: 24-cycle command base + 1 cycle per filled
    // pixel (zoomed fill). draw_text/draw_party_text/draw_dot_line
    // all flow through here, so every glyph in the frame is charged.
    g->cpu_cycles += 24 + (int)( w * h );
}

// dotted line of tiny glyphs between two points (tunnel edges, rings).
// HARD CAP on dots: the Vircon32 GPU fits a limited number of draw
// calls per frame -- going over stalls the emulator (100% CPU) and the
// additive glow accumulates into a white-out. 24 dots max.
void draw_dot_line( G* g, float x0, float y0, float x1, float y1, int step )
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = dx * dx + dy * dy;
    int n = (int)( len / ( step * step ) );
    if( n < 1 ) n = 1;
    if( n > 24 ) n = 24;
    int i;
    for( i = 0; i <= n; i++ )
    {
        float t = ( i * 1.0 ) / n;
        // alternate glyph sizes for a shimmer
        float s = ( ( i + g->frame ) & 1 ) ? 2.0 : 3.0;
        draw_glyph( g, '.', x0 + dx * t, y0 + dy * t, s, s );
    }
}

int string_len( char* s )
{
    int n = 0;
    while( s[ n ] != 0 ) n++;
    return n;
}

// ---------------------------------------------------------------------------
//  Vector-style web outlines
// ---------------------------------------------------------------------------

//  Best-match angle for direction (dx,dy), as a 256-step turn index:
//  coarse 32-step scan of the direction tables, then a +-4 refine
//  around the winner. Multiply by 2*PI/256 (0.024543692) for radians.
//  Convention matches set_drawing_angle: in y-down screen coords the
//  resulting angle grows clockwise, which is what the GPU expects.
//
//  FAST PATH (was a 32-step coarse scan + 8-step refine: 40 dot
//  products, the single biggest CPU cost in busy frames -- every
//  spoke, spike and shockwave bar paid it). Now: a polynomial atan2
//  guess (max error ~0.004 rad, a sixth of one table step), rounded
//  to the nearest index, then the SAME dot-product test on that index
//  and its two neighbours. The best of three always contains the scan's
//  answer, so the result is unchanged -- for 3 dot products instead of
//  40.
int dir_angle( G* g, float dx, float dy )
{
    float ax = dx;
    if( ax < 0 ) ax = -ax;
    float ay = dy;
    if( ay < 0 ) ay = -ay;
    if( ax + ay < 0.000001 ) return 0;
    // atan of the octant ratio z = min/max in [0,1]:
    //   atan(z) ~= z * (pi/4 + 0.273 * (1 - z))
    float a;
    if( ax >= ay )
    {
        float z = ay / ax;
        a = z * ( 0.7853982 + 0.273 * ( 1.0 - z ) );
    }
    else
    {
        float z = ax / ay;
        a = 1.5707963 - z * ( 0.7853982 + 0.273 * ( 1.0 - z ) );
    }
    if( dx < 0 ) a = 3.14159265 - a;
    if( dy < 0 ) a = -a;
    int guess = TURN_INDEX( a );
    int best = guess;
    float bestdot = g->COS_TABLE[ guess ] * dx + g->SIN_TABLE[ guess ] * dy;
    int j = ( guess + 255 ) & 255;
    float dot = g->COS_TABLE[ j ] * dx + g->SIN_TABLE[ j ] * dy;
    if( dot > bestdot ) { bestdot = dot; best = j; }
    j = ( guess + 1 ) & 255;
    dot = g->COS_TABLE[ j ] * dx + g->SIN_TABLE[ j ] * dy;
    if( dot > bestdot ) best = j;
    return best;   // 256-step turn index; caller converts to radians
}

//  Solid bar from (x0,y0) to (x1,y1), thickness in pixels. BIOS font
//  REGION 20 is a solid 10x20 block with its DEFAULT hotspot at the
//  TOP-LEFT (print_at draws text top-left referenced, so all BIOS
//  regions are defined that way). NEVER touch region 20's bounds or
//  hotspot: redefining them (define_region / set_region_hotspot)
//  replaces the BIOS's correct geometry with guessed coordinates and
//  the draws sample the texture's magenta region-outline lines
//  instead of the solid block (empirically confirmed).
//
//  Placement with a top-left hotspot: rotation pivots at the draw
//  point, so we draw at (start + half-thickness perpendicular offset),
//  which centers the bar on the line. Angle in RADIANS (video.h).
//
//  SOLID-alpha only — additive bars would re-trigger the emulator's
//  BlendAdd saturation bug. Invalidates draw_glyph's region/scale
//  cache, since this bypasses it and changes GPU state directly.
// full-precision bar angle: dir_angle's table scan plus the
// sub-index refine — factored out of draw_segment so the web bar
// angle cache (see render_web) can compute an angle WITHOUT drawing
float segment_angle( G* g, float dx, float dy )
{
    float len = sqrt( dx * dx + dy * dy );
    if( len < 2.0 ) return 0;
    float idx = dir_angle( g, dx, dy );   // integer turn index
    // SUB-INDEX REFINEMENT: the perpendicular component of (dx,dy)
    // relative to the table direction equals sin(angle error), which
    // for our small residual (<0.7 deg) is the error in radians.
    // Adding it gives a near-exact angle, killing vertex drift.
    float c0 = g->COS_TABLE[ (int)idx ];
    float s0 = g->SIN_TABLE[ (int)idx ];
    return idx * 0.024543692 + ( -s0 * dx + c0 * dy ) / len;
}

void draw_segment( G* g, float x0, float y0, float x1, float y1, float thick )
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrt( dx * dx + dy * dy );
    if( len < 2.0 ) return;

    float a = segment_angle( g, dx, dy );
    float c = cos32( g, a );
    float s = sin32( g, a );

    select_region( 20 );   // solid block, DEFAULT hotspot (top-left)
    set_drawing_scale( len / 10.0 + 0.3, thick / 20.0 );
    set_drawing_angle( a );
    // bar extends from the draw point along (c,s) for `len` and
    // perpendicular for `thick`; offsetting the draw point by half the
    // thickness along the perpendicular centers the bar on the line
    draw_region_rotozoomed_at( (int)( x0 + s * thick * 0.5 ),
                               (int)( y0 - c * thick * 0.5 ) );
    // CPU meter: rotozoomed fill charged DOUBLE (the known
    // slow path) over the bar's length*thickness pixel area
    g->cpu_cycles += 24 + (int)( len * thick * 2.0 );
    set_drawing_angle( 0 );
    g->last_region = -1;   // draw_glyph cache is stale now
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
}

// draw_segment with the angle SUPPLIED — the web bar angle cache
// (see render_web) precomputes each level's rim/cap bar directions
// once, so the hot per-frame path skips segment_angle's dir_angle
// scan entirely. Identical draw path to draw_segment otherwise; the
// caller passes the cached angle. Length is still measured per call
// (it changes with the warp fly factor and the jump pull-back).
void draw_segment_c( G* g, float x0, float y0, float x1, float y1, float thick, float a )
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrt( dx * dx + dy * dy );
    if( len < 2.0 ) return;

    float c = cos32( g, a );
    float s = sin32( g, a );

    select_region( 20 );   // solid block, DEFAULT hotspot (top-left)
    set_drawing_scale( len / 10.0 + 0.3, thick / 20.0 );
    set_drawing_angle( a );
    draw_region_rotozoomed_at( (int)( x0 + s * thick * 0.5 ),
                               (int)( y0 - c * thick * 0.5 ) );
    // CPU meter: rotozoomed fill charged double (slow path)
    g->cpu_cycles += 24 + (int)( len * thick * 2.0 );
    set_drawing_angle( 0 );
    g->last_region = -1;   // draw_glyph cache is stale now
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
}

// draw BIOS region c (a 10x20 cell) CENTRED at (x,y) and rotated a
// radians clockwise (y-down screen) — via the ROTOZOOMED command, the
// same proven path draw_segment uses. The draw pivots on the region
// HOTSPOT (top-left), so the local centre (w/2, h/2) is rotated and
// subtracted to land the glyph centred on (x,y).
void draw_rot_glyph( G* g, int c, float x, float y, float w, float h, float a )
{
    select_region( c );
    set_drawing_scale( w / 10.0, h / 20.0 );
    set_drawing_angle( a );
    float ca = cos32( g, a );
    float sa = sin32( g, a );
    float ox = ( w / 2 ) * ca - ( h / 2 ) * sa;
    float oy = ( w / 2 ) * sa + ( h / 2 ) * ca;
    draw_region_rotozoomed_at( (int)( x - ox ), (int)( y - oy ) );
    // CPU meter: rotozoomed fill charged double (slow path)
    g->cpu_cycles += 24 + (int)( w * h * 2.0 );
    set_drawing_angle( 0 );
    g->last_region = -1;
    g->last_scale_x = -9999.0;
    g->last_scale_y = -9999.0;
}

void draw_party_text( G* g, char* s, float x, float y, float size, int huebase )
{
    int i = 0;
    // measured once, not per letter (that was quadratic in the length)
    float left = x - string_len( s ) * 0.5 * size * 0.62;
    while( s[ i ] != 0 )
    {
        float wob = SIN32( g, i * 0.35 + g->frame * 0.05 ) * size * 0.28;
        set_multiply_color( hue( huebase + i * 9 + ( g->frame >> 1 ) ) );
        draw_glyph( g, s[ i ], left + i * size * 0.62, y + wob, size, size * 1.15 );
        i++;
    }
}

void draw_text( G* g, char* s, float x, float y, float size, int c )
{
    int i = 0;
    while( s[ i ] != 0 )
    {
        set_multiply_color( c );
        // VERTICAL OVERSAMPLE: the BIOS font cell is 20 rows tall and
        // the letter art spans nearly all of them. Drawing a cell at
        // fewer output rows than the art makes the GPU sampler DROP
        // source rows — the top and/or bottom of every letter gets
        // shaved (the "text clips everywhere" bug). Drawing the cell
        // 1.2x taller than the nominal size keeps every art row, same
        // trick draw_party_text already used.
        //
        // HORIZONTAL: advance widened 0.62 -> 0.78. The letter art
        // fills about 7/10 of the 10-wide cell, so at 0.62*size the
        // next glyph started before the previous letter's art ended —
        // everything read as squeezed. 0.78 leaves a clear gap
        // between letters. All centered call sites are positioned for
        // this width.
        draw_glyph( g, s[ i ], x + i * size * 0.78, y, size, size * 1.2 );
        i++;
    }
}

// decimal string of n in out (out must hold >= 9 chars: max 7 digits + 0)
void score_str( int n, char* out )
{
    char tmp[ 12 ];
    int i = 0;
    int len = 0;
    if( n <= 0 ) { out[ 0 ] = '0'; out[ 1 ] = 0; return; }
    while( n > 0 && i < 11 )
    {
        tmp[ i ] = '0' + n % 10;
        n /= 10;
        i++;
    }
    len = i;
    for( i = 0; i < len; i++ ) out[ i ] = tmp[ len - 1 - i ];
    out[ len ] = 0;
}

// REAL hardware GPU pixel usage: the GPU_RemainingPixels port (the
// same SDK asm-reader pattern as get_multiply_color — value returns
// through R0). The counter refills at vsync with the frame's pixel
// budget (2,073,600 on the 640x360 screen) and counts down as the
// GPU fills pixels, so (frame-start reading - current) = pixels used.
int gpu_remaining_pixels()
{
    asm
    {
        "in R0, GPU_RemainingPixels"
    }
}

// one meter line: "XYZ E: 61k 24%" — k-precision total plus percent
// of the given budget, drawn at (8, y). All integer math; tmp holds
// up to 4 k-digits (GPU pixel totals can exceed 999k).
void meter_line( G* g, char l0, char l1, char l2, char tag, int total, int budget, int y )
{
    char buf[ 20 ];
    buf[ 0 ] = l0; buf[ 1 ] = l1; buf[ 2 ] = l2; buf[ 3 ] = ' ';
    buf[ 4 ] = tag; buf[ 5 ] = ':'; buf[ 6 ] = ' ';
    int p = 7;
    char tmp[ 5 ];
    int n = 0;
    int v = total / 1000;
    if( v == 0 ) { tmp[ 0 ] = '0'; n = 1; }
    while( v > 0 && n < 4 )
    {
        tmp[ n ] = '0' + v % 10;
        v /= 10;
        n++;
    }
    int q;
    for( q = 0; q < n; q++ ) buf[ p + q ] = tmp[ n - 1 - q ];
    p += n;
    buf[ p ] = 'k'; p++;
    buf[ p ] = ' '; p++;
    int pct = total * 100 / budget;
    if( pct > 99 ) pct = 99;
    buf[ p ] = '0' + ( pct / 10 ) % 10; p++;
    buf[ p ] = '0' + pct % 10; p++;
    buf[ p ] = '%'; p++;
    buf[ p ] = 0;
    draw_text( g, buf, 8, y, 10, RGB( 200, 210, 230 ) );
}

// DEBUG METERS — a "likely busy-ness" sight indicator, drawn as TWO
// lines top-left. The CPU line is the heuristic cost model: every
// instrumented draw charges 24 cycles per command plus 1 cycle per
// filled pixel (rotozoomed fills — web bars, rotated claws — cost
// double, the GPU's documented slow path), against the 250,000-
// CYCLE CPU frame budget (the CPU's per-frame cycle allowance at
// 60 fps — the CPU cycles, NOT GPU cycles). The GPU line is REAL
// hardware usage: pixels consumed this frame out of the frame's
// opening GPU_RemainingPixels reading (self-calibrating budget).
//
// PARITY READOUT: the game deliberately halves its draws on
// alternating frames (stars, rings, particles parity-split), so a
// single number hides half the story. Each frame's totals are filed
// under their parity, and every half second (30 frames) the
// readout SWAPS which parity it shows, labelled E: / O: — one
// half-second of even frames, the next of odd frames.
//
// Both meters sample BEFORE drawing themselves, so the readout's
// own cost never feeds back into the numbers.
void gpu_meter( G* g )
{
    if( g->debug_gpu == 0 ) return;
    // real GPU pixels used so far this frame
    int remaining = gpu_remaining_pixels();
    int gused = g->gpixels_start - remaining;
    if( gused < 0 ) gused = 0;
    // file this frame's totals under their parity
    if( ( g->frame & 1 ) == 0 )
    {
        g->cpu_even = g->cpu_cycles;
        g->gpu_pix_even = gused;
    }
    else
    {
        g->cpu_odd = g->cpu_cycles;
        g->gpu_pix_odd = gused;
    }
    // every 30 frames: swap which parity the readout shows
    if( g->frame % 30 == 0 )
    {
        if( g->cpu_parity == 0 ) g->cpu_parity = 1;
        else                     g->cpu_parity = 0;
    }
    int cputot = g->cpu_even;
    int gputot = g->gpu_pix_even;
    char tag = 'E';
    if( g->cpu_parity == 1 )
    {
        cputot = g->cpu_odd;
        gputot = g->gpu_pix_odd;
        tag = 'O';
    }
    meter_line( g, 'C', 'P', 'U', tag, cputot, 250000, 8 );
    meter_line( g, 'G', 'P', 'U', tag, gputot, g->gpixels_start, 21 );
}
