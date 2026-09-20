#title "Star Raiders"
#version 0.1

#include "video.h"
#include "input.h"
#include "string.h"
#include "time.h"

// ============================================================================
//  STAR RAIDERS for Vircon32 -- phase 1: scrolling quadrant starfield
//
//  Written to the v32c++ subset (same conventions as the Space Invaders
//  project): no templates, no static members, no in-class initializers,
//  no class-nested enums, no 'unsigned', no ternaries, one-word parameter
//  and return types, no bare constructor/functional casts. SDK calls are
//  passed through undeclared to the Vircon32 C compiler.
//
//  Graphics use ONLY the BIOS font (texture -1): one pre-defined region
//  per ASCII code, 10x20 pixel characters. Stars are the '.' glyph
//  (region 46), the reticle is '+' (region 43). The glyphs are drawn as
//  regions and ZOOMED via set_drawing_scale + draw_region_zoomed_at to
//  fake depth and warp speed. Note: print_at selects texture -1 itself
//  and restores the previous texture, so it is safe inside our loop.
//
//  Text: Vircon strings are int arrays (one int per character, null
//  terminated) -- see string.h. We use string.h's strcpy/strcat/itoa.
//  Colors: video.h has no get_color; use make_color_rgb / color_*.
// ============================================================================

// NOTE on constants: the Vircon32 C compiler enforces const very
// strictly (a const int global cannot even be assigned FROM), so all
// tunables are #defines instead of const variables.

#define SCREEN_W   640
#define SCREEN_H   360
#define CENTER_X   320
#define CENTER_Y   180

// BIOS font: texture -1, one region per ASCII code, chars are 10x20 px.
// Region id for an ASCII code is simply the code itself.
#define ASCII_DOT   46    // '.'
#define ASCII_PLUS  43    // '+'

// galaxy structure: 8x8 quadrants, each 8x8 sectors
#define GALAXY_QUADS       8
#define SECTORS_PER_QUAD   8
#define QUAD_SIZE   1024          // quadrant space, abstract units
#define QUAD_HALF    512

#define STAR_COUNT        140
#define CRUISE_SPEED        5     // units per frame at parallax 1.0

#define WARP_FRAMES       100     // length of a warp jump
#define WARP_GROWTH      8.0     // how violently stars stream outward

// ---------------------------------------------------------------------------
//  HUD text. Vircon string literals are const int* and the console C
//  compiler will not pass them to strcpy/strcat (int* parameters), so
//  every literal lives in a named int array (same pattern as string.h's
//  own itoa, which builds "0123456789ABCDEF" in an int array).
// ---------------------------------------------------------------------------

int hud_line[64];
int hud_num[16];

// NOTE: the v32c++ grammar does not accept a string literal as an
// array initializer ("int s[4] = \"abc\";" fails -- only brace lists
// are allowed there), so these are written as ASCII code lists.
// (Text: "QUADRANT ", ",", "  SECTOR ", "ENGINES: WARP",
//  "ENGINES: CRUISE", "ENGINES: OFF", "  HEAD: ", "N", "E", "S", "W")
int s_quadrant[10] = { 81, 85, 65, 68, 82, 65, 78, 84, 32, 0 };
int s_comma[2]     = { 44, 0 };
int s_sector[9]    = { 32, 32, 83, 69, 67, 84, 79, 82, 0 };
int s_warp[15]     = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 87, 65, 82, 80, 0 };
int s_cruise[17]   = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 67, 82, 85, 73, 83, 69, 0 };
int s_off[13]      = { 69, 78, 71, 73, 78, 69, 83, 58, 32, 79, 70, 70, 0 };
int s_head[9]      = { 32, 32, 72, 69, 65, 68, 58, 32, 0 };
int s_n[2] = { 78, 0 };
int s_e[2] = { 69, 0 };
int s_s[2] = { 83, 0 };
int s_w[2] = { 87, 0 };

// appends a number to the HUD line (itoa into a temp, then strcat;
// avoids pointer arithmetic on the destination array entirely)
void hud_append_int( int v )
{
    itoa( v, hud_num, 10 );
    strcat( hud_line, hud_num );
}

// ---------------------------------------------------------------------------
//  Deterministic RNG (own LCG, independent of the hardware RNG)
// ---------------------------------------------------------------------------

class RNG
{
public:
    int state;

    void seed( int s )
    {
        if (s == 0)
            s = 1;
        state = s;
    }

    int next()
    {
        state = state * 1103515245 + 12345;
        return (state >> 16) & 32767;
    }

    int between( int lo, int hi )  // inclusive
    {
        return lo + (next() % (hi - lo + 1));
    }
};

// ---------------------------------------------------------------------------
//  A single star: fixed position inside quadrant space plus a parallax
//  factor p in (0..1]. Rendered as a zoomed '.' region.
// ---------------------------------------------------------------------------

class Star
{
public:
    float fx;    // quadrant-space position
    float fy;
    float p;     // parallax / depth: 0 = infinitely far, 1 = ship's plane
    int tint;    // 0 white, 1 blue, 2 warm, 3 pale cyan

    void randomize( RNG *rng )
    {
        fx = rng->between( 0, QUAD_SIZE - 1 );
        fy = rng->between( 0, QUAD_SIZE - 1 );
        p = rng->between( 2, 10 ) * 0.1;   // 0.2 .. 1.0
        tint = rng->between( 0, 3 );
    }
};

// ---------------------------------------------------------------------------
//  The starfield: one deterministic star pattern per galaxy quadrant.
//  The ship scrolls through quadrant space with the D-pad (parallax by
//  star depth), and the A button performs a warp jump: stars zoom and
//  stream outward from screen center, then we arrive in the adjacent
//  quadrant.
// ---------------------------------------------------------------------------

class Starfield
{
public:
    Star stars[140];     // STAR_COUNT (literal size for the transpiler)
    RNG rng;

    int qx;              // current quadrant, galaxy coords 0..7
    int qy;
    float shipx;         // ship position in quadrant space
    float shipy;
    int heading;         // 0=N 1=E 2=S 3=W, last travel direction
    int warp_t;          // 0 = idle, else 1..WARP_FRAMES
    int warp_dir;        // quadrant step direction of current warp

    // ------------------------------------------------------------------

    void enter_quadrant( int nqx, int nqy )
    {
        int i;
        qx = nqx;
        qy = nqy;
        // deterministic pattern: same quadrant always shows the same stars
        rng.seed( (qy * GALAXY_QUADS + qx + 1) * 7919 );
        i = 0;
        while (i < STAR_COUNT)
        {
            stars[i].randomize( &rng );
            i = i + 1;
        }
    }

    void init( int start_qx, int start_qy )
    {
        heading = 0;
        warp_t = 0;
        warp_dir = 0;
        shipx = QUAD_HALF;
        shipy = QUAD_HALF;
        enter_quadrant( start_qx, start_qy );
    }

    // wrap a quadrant-space delta to (-QUAD_HALF, QUAD_HALF]
    float wrap_delta( float d )
    {
        while (d >= QUAD_HALF)
            d = d - QUAD_SIZE;
        while (d < -QUAD_HALF)
            d = d + QUAD_SIZE;
        return d;
    }

    // ------------------------------------------------------------------

    void update()
    {
        int dx;
        int dy;
        dx = 0;
        dy = 0;
        if (gamepad_left() != 0)
            dx = -1;
        if (gamepad_right() != 0)
            dx = 1;
        if (gamepad_up() != 0)
            dy = -1;
        if (gamepad_down() != 0)
            dy = 1;

        // warp jump: A button starts it, direction = held pad (or heading)
        if (warp_t == 0)
        {
            if (gamepad_button_a() != 0)
            {
                warp_t = 1;
                warp_dir = heading;
                if (dy < 0)
                    warp_dir = 0;
                if (dx > 0)
                    warp_dir = 1;
                if (dy > 0)
                    warp_dir = 2;
                if (dx < 0)
                    warp_dir = 3;
            }
            else
            {
                // normal cruising: scroll through quadrant space
                if (dx != 0 || dy != 0)
                {
                    shipx = shipx + dx * CRUISE_SPEED;
                    shipy = shipy + dy * CRUISE_SPEED;
                    if (dy < 0)
                        heading = 0;
                    if (dx > 0)
                        heading = 1;
                    if (dy > 0)
                        heading = 2;
                    if (dx < 0)
                        heading = 3;
                }
            }
        }
        else
        {
            // in warp: hold course, count frames, arrive at the end
            warp_t = warp_t + 1;
            if (warp_t > WARP_FRAMES)
            {
                warp_t = 0;
                arrive();
            }
        }

        // keep ship inside its quadrant (space wraps)
        while (shipx < 0)
            shipx = shipx + QUAD_SIZE;
        while (shipx >= QUAD_SIZE)
            shipx = shipx - QUAD_SIZE;
        while (shipy < 0)
            shipy = shipy + QUAD_SIZE;
        while (shipy >= QUAD_SIZE)
            shipy = shipy - QUAD_SIZE;
    }

    // warp finished: step to the adjacent quadrant, entering from the
    // edge opposite to our direction of travel
    void arrive()
    {
        int nqx;
        int nqy;
        nqx = qx;
        nqy = qy;
        if (warp_dir == 0)
        {
            nqy = qy - 1;
            shipy = QUAD_SIZE - 64;
        }
        else if (warp_dir == 1)
        {
            nqx = qx + 1;
            shipx = 64;
        }
        else if (warp_dir == 2)
        {
            nqy = qy + 1;
            shipy = 64;
        }
        else
        {
            nqx = qx - 1;
            shipx = QUAD_SIZE - 64;
        }
        // galaxy wraps around
        while (nqx < 0)
            nqx = nqx + GALAXY_QUADS;
        while (nqx >= GALAXY_QUADS)
            nqx = nqx - GALAXY_QUADS;
        while (nqy < 0)
            nqy = nqy + GALAXY_QUADS;
        while (nqy >= GALAXY_QUADS)
            nqy = nqy - GALAXY_QUADS;
        enter_quadrant( nqx, nqy );
    }

    // ------------------------------------------------------------------

    void draw()
    {
        int i;
        float g;        // warp zoom growth factor (1 = normal)
        g = 1;
        if (warp_t > 0)
        {
            float t;
            t = warp_t;
            t = t / WARP_FRAMES;
            g = 1 + t * t * WARP_GROWTH;
        }

        clear_screen( make_color_rgb( 2, 4, 12 ) );

        // stars: '.' regions of the BIOS font. The dot inside the 10x20
        // glyph is only a few pixels, so modest scale factors (1..12)
        // give dot sizes from ~2 to ~30 px.
        select_texture( -1 );
        select_region( ASCII_DOT );
        set_blending_mode( blending_add );

        i = 0;
        while (i < STAR_COUNT)
        {
            float ox;
            float oy;
            float dx;
            float dy;
            int sx;
            int sy;
            float s;

            dx = wrap_delta( stars[i].fx - shipx );
            dy = wrap_delta( stars[i].fy - shipy );

            // project: parallax by depth, zoom outward during warp
            ox = dx * stars[i].p * g;
            oy = dy * stars[i].p * g;

            // cull off-screen (they fly out fast during warp)
            if (ox > -360 && ox < 360 && oy > -210 && oy < 210)
            {
                int b;
                int r;
                int gg;
                int bb;
                b = 70 + stars[i].p * 185;
                r = b;
                gg = b;
                bb = b;
                if (stars[i].tint == 1)
                {
                    r = b * 0.7;
                    gg = b * 0.85;
                    bb = b + 40;
                }
                else if (stars[i].tint == 2)
                {
                    r = b + 30;
                    gg = b * 0.8;
                    bb = b * 0.7;
                }
                else if (stars[i].tint == 3)
                {
                    r = b * 0.7;
                    gg = b + 20;
                    bb = b + 30;
                }
                if (r > 255)
                    r = 255;
                if (gg > 255)
                    gg = 255;
                if (bb > 255)
                    bb = 255;

                sx = CENTER_X + ox;
                sy = CENTER_Y + oy;

                // dot size grows with depth and with warp zoom
                s = 1.0 + stars[i].p * 2.0;
                s = s * g;
                if (s > 12.0)
                    s = 12.0;

                set_multiply_color( make_color_rgb( r, gg, bb ) );
                set_drawing_scale( s, s );
                draw_region_zoomed_at( sx, sy );
            }
            i = i + 1;
        }

        set_blending_mode( blending_alpha );
        draw_reticle();
        draw_hud();
    }

    void draw_reticle()
    {
        select_region( ASCII_PLUS );
        set_multiply_color( make_color_rgb( 0, 200, 120 ) );
        set_drawing_scale( 1.0, 1.0 );
        draw_region_zoomed_at( CENTER_X, CENTER_Y );
    }

    void draw_hud()
    {
        set_multiply_color( color_white );

        // top-left: position readout
        strcpy( hud_line, s_quadrant );
        hud_append_int( qx + 1 );
        strcat( hud_line, s_comma );
        hud_append_int( qy + 1 );
        strcat( hud_line, s_sector );
        hud_append_int( sector_x() + 1 );
        strcat( hud_line, s_comma );
        hud_append_int( sector_y() + 1 );
        print_at( 8, 8, hud_line );

        // bottom-left: engines + heading
        if (warp_t > 0)
            strcpy( hud_line, s_warp );
        else if (gamepad_left() != 0 || gamepad_right() != 0 ||
                 gamepad_up() != 0 || gamepad_down() != 0)
            strcpy( hud_line, s_cruise );
        else
            strcpy( hud_line, s_off );
        strcat( hud_line, s_head );
        if (heading == 0)
            strcat( hud_line, s_n );
        else if (heading == 1)
            strcat( hud_line, s_e );
        else if (heading == 2)
            strcat( hud_line, s_s );
        else
            strcat( hud_line, s_w );
        print_at( 8, 332, hud_line );
    }

    int sector_x()
    {
        int s;
        s = shipx / (QUAD_SIZE / SECTORS_PER_QUAD);
        if (s < 0)
            s = 0;
        if (s > SECTORS_PER_QUAD - 1)
            s = SECTORS_PER_QUAD - 1;
        return s;
    }

    int sector_y()
    {
        int s;
        s = shipy / (QUAD_SIZE / SECTORS_PER_QUAD);
        if (s < 0)
            s = 0;
        if (s > SECTORS_PER_QUAD - 1)
            s = SECTORS_PER_QUAD - 1;
        return s;
    }
};

Starfield g_field;

// ---------------------------------------------------------------------------
//  Main loop
// ---------------------------------------------------------------------------

void main( void )
{
    g_field.init( 3, 4 );          // start somewhere in the middle

    while (1)
    {
        g_field.update();
        g_field.draw();
        end_frame();
    }
}
