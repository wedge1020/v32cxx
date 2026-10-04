/* ****************************************************************************
 *  libc/v32term.c -- see v32term.h. Included once, by v32libc.c.
 * ****************************************************************************/
#ifndef V32TERM_C
#define V32TERM_C

#include "v32term.h"

/* ---- the screen ----------------------------------------------------------- */

static int v32term_want[V32TERM_CELLS];     /* what should be on screen */
static int v32term_shown[V32TERM_CELLS];    /* what is; -1 = unknown */
static int v32term_ready = 0;
static int v32term_dirty = 0;               /* any cell differing? */
static int v32term_dirty_first = 0;         /* ...and if so, somewhere in */
static int v32term_dirty_last = 0;          /*    this range of cells     */

static void v32term_dirty_all(void)
{
    v32term_dirty = 1;
    v32term_dirty_first = 0;
    v32term_dirty_last = V32TERM_CELLS - 1;
}

static int v32term_tty_y = 0;
static int v32term_tty_x = 0;
static int v32term_tty_attr = 0;

/* Foreground colours, indexed by V32TERM_INK(). Vircon32 colours are
 * 0xAABBGGRR. */
static int v32term_palette[16] = {
    0xFFC8C8C8,     /* 0  default: soft white */
    0xFF4040F0,     /* 1  red */
    0xFF40D040,     /* 2  green */
    0xFF40E0F0,     /* 3  yellow */
    0xFFF09050,     /* 4  blue */
    0xFFE060E0,     /* 5  magenta */
    0xFFE0D040,     /* 6  cyan */
    0xFFFFFFFF,     /* 7  white */
    0xFF909090,     /* 8  gray */
    0xFF2090F8,     /* 9  orange */
    0xFF3070B0,     /* 10 brown */
    0xFF606060,     /* 11 dim */
    0xFF308830,     /* 12 dark green */
    0xFFC8C8C8, 0xFFC8C8C8, 0xFFC8C8C8
};

/* Pixels the GPU can still draw this frame. Past its budget the GPU drops
 * draw commands until the next frame, so a long repaint has to pause. */
static int v32term_gpu_room(void)
{
    asm { "in R0, GPU_RemainingPixels" }
}

void v32term_redraw(void)
{
    int i;

    for (i = 0; i < V32TERM_CELLS; i++)
        v32term_shown[i] = -1;
    v32term_dirty_all();
}

void v32term_clear(void)
{
    int i;

    for (i = 0; i < V32TERM_CELLS; i++)
        v32term_want[i] = ' ';
    v32term_dirty_all();
}

/* ---- the 8x15 character cell ---------------------------------------------
 *
 * A BIOS glyph is a 10x20 region of the BIOS texture (texture -1; region
 * N is character N; where it lies is read from the BIOS, never assumed). Its
 * ink never touches the first three rows, sits in rows 3..15 for every
 * character's body, and only g j p q y , Q and _ reach below that.
 *
 * Simply drawing the region at 0.8 x 0.75 scale does not work: the GPU
 * samples nearest-neighbour, so a quarter of the rows are skipped
 * outright, and with them the whole of '-', the bar of '+' and of 'H'.
 * Instead the cell is built from regions of its own, defined here on the
 * BIOS texture (a cartridge may define regions on it; ids below 256 are
 * the BIOS's and are left alone):
 *
 *   - the body: rows 3..15 of the glyph, all 13 of them, drawn at their
 *     own height. Only the width is scaled, 10 -> 8, which thins the
 *     two-pixel stems to one and leaves a pixel of space either side;
 *   - a tail, for the eight characters that have one, in the cell's last
 *     two rows. For , Q and _ that is rows 16..17 as they are. The
 *     descenders of g j p q y are four rows (16..19) and only two fit:
 *     the first, which carries the stem or the sides of the loop, and the
 *     last, which closes it (for y, the last two: its hook). Scaling the
 *     four rows to half height instead lets the GPU drop the closing row,
 *     and g, j and y lose their shape;
 *   - the paper: 10x15 of the solid block (character 20).
 */
#define V32TERM_BODY_TOP    3           /* first glyph row with ink */
#define V32TERM_BODY_ROWS   13
#define V32TERM_REG_BODY    256         /* + character */
#define V32TERM_REG_TAIL    512         /* + character */
#define V32TERM_REG_PAPER   768
#define V32TERM_REG_SHADE   769         /* + (character - 17) * 2 + (row & 1) */
#define V32TERM_REG_TAIL2   1024        /* + character */

/* 0 = no tail, 1 = rows 16..17 as they are, 2 = two rows picked from 16..19 */
static int v32term_tail_kind(int ch)
{
    if (ch == 'g' || ch == 'j' || ch == 'p' || ch == 'q' || ch == 'y')
        return 2;
    if (ch == ',' || ch == 'Q' || ch == '_')
        return 1;
    return 0;
}

/* Where the BIOS itself says the selected region is. The glyphs are never
 * located by fixed texture coordinates: BIOS versions have moved the font
 * within the texture (1.0 -> 1.1 did), but region N is character N in all
 * of them. */
static int v32term_region_min_x(void)
{
    asm { "in R0, GPU_RegionMinX" }
}

static int v32term_region_min_y(void)
{
    asm { "in R0, GPU_RegionMinY" }
}

static void v32term_define_font(void)
{
    int ch, x, y, kind;

    select_texture(-1);
    for (ch = 33; ch < 127; ch++) {
        select_region(ch);
        x = v32term_region_min_x();
        y = v32term_region_min_y();
        select_region(V32TERM_REG_BODY + ch);
        define_region(x, y + V32TERM_BODY_TOP, x + 9,
                      y + V32TERM_BODY_TOP + V32TERM_BODY_ROWS - 1,
                      x, y + V32TERM_BODY_TOP);
        kind = v32term_tail_kind(ch);
        if (kind == 1) {
            select_region(V32TERM_REG_TAIL + ch);
            define_region(x, y + 16, x + 9, y + 17, x, y + 16);
        } else if (kind == 2) {
            /* two chosen rows, each a region one row high */
            int first = (ch == 'y') ? 18 : 16;

            select_region(V32TERM_REG_TAIL + ch);
            define_region(x, y + first, x + 9, y + first, x, y + first);
            select_region(V32TERM_REG_TAIL2 + ch);
            define_region(x, y + 19, x + 9, y + 19, x, y + 19);
        }
    }
    select_region(20);                  /* character 20: the solid block */
    x = v32term_region_min_x();
    y = v32term_region_min_y();
    select_region(V32TERM_REG_PAPER);
    define_region(x, y, x + 9, y + V32TERM_CELL_H - 1, x, y);
    /* The shade blocks, characters 17..20: three dither patterns and the
     * solid block. A cell is cut straight out of the pattern, 8x15, not
     * scaled (scaling a dither ruins it). The patterns repeat every two
     * rows and a cell has 15, so odd screen rows start one row down:
     * that keeps the pattern unbroken from one row of cells to the next. */
    for (ch = V32TERM_SHADE_FIRST; ch <= V32TERM_SHADE_LAST; ch++) {
        select_region(ch);
        x = v32term_region_min_x();
        y = v32term_region_min_y();
        for (kind = 0; kind < 2; kind++) {
            select_region(V32TERM_REG_SHADE + (ch - V32TERM_SHADE_FIRST) * 2 + kind);
            define_region(x, y + kind, x + V32TERM_CELL_W - 1, y + kind + V32TERM_CELL_H - 1,
                          x, y + kind);
        }
    }
}

void v32term_init(void)
{
    if (v32term_ready)
        return;
    v32term_ready = 1;
    v32term_define_font();
    v32term_clear();
    v32term_redraw();
    v32term_tty_y = 0;
    v32term_tty_x = 0;
    v32term_tty_attr = 0;
    select_gamepad(0);
}

void v32term_set(int y, int x, int cell)
{
    int i;

    if (y < 0 || y >= V32TERM_ROWS || x < 0 || x >= V32TERM_COLS)
        return;
    i = y * V32TERM_COLS + x;
    if (v32term_want[i] != cell) {
        v32term_want[i] = cell;
        if (!v32term_dirty) {
            v32term_dirty = 1;
            v32term_dirty_first = i;
            v32term_dirty_last = i;
        } else {
            if (i < v32term_dirty_first)
                v32term_dirty_first = i;
            if (i > v32term_dirty_last)
                v32term_dirty_last = i;
        }
    }
}

int v32term_get(int y, int x)
{
    if (y < 0 || y >= V32TERM_ROWS || x < 0 || x >= V32TERM_COLS)
        return ' ';
    return v32term_want[y * V32TERM_COLS + x];
}

/* Looks for `text` on rows first_row..last_row of the screen (it must
 * fit on one row). Programs that drive an old text interface from a
 * gamepad read the screen to see what is being asked; this is the fast
 * way to. */
int v32term_find(char *text, int first_row, int last_row)
{
    int y, x, k, len;
    int first = text[0];
    int *cell;

    len = 0;
    while (text[len] != '\0')
        len++;
    if (len == 0 || len > V32TERM_COLS)
        return 0;
    if (first_row < 0)
        first_row = 0;
    if (last_row >= V32TERM_ROWS)
        last_row = V32TERM_ROWS - 1;
    for (y = first_row; y <= last_row; y++) {
        cell = &v32term_want[y * V32TERM_COLS];
        for (x = 0; x <= V32TERM_COLS - len; x++) {
            if ((cell[x] & V32TERM_CHAR) != first)
                continue;
            for (k = 1; k < len; k++)
                if ((cell[x + k] & V32TERM_CHAR) != text[k])
                    break;
            if (k == len)
                return 1;
        }
    }
    return 0;
}

void v32term_flush(void)
{
    int i, cell, ch, ink, paper, swap, px, py, kind;

    if (!v32term_ready)
        v32term_init();
    if (!v32term_dirty)
        return;
    select_texture(-1);
    set_drawing_scale(0.8, 1.0);
    px = (v32term_dirty_first % V32TERM_COLS) * V32TERM_CELL_W;
    py = (v32term_dirty_first / V32TERM_COLS) * V32TERM_CELL_H;
    for (i = v32term_dirty_first; i <= v32term_dirty_last; i++) {
        cell = v32term_want[i];
        if (cell != v32term_shown[i]) {
            /* a cell is at most three small quads; past its per-frame
             * pixel budget the GPU would silently drop them */
            if (v32term_gpu_room() < 2000)
                end_frame();
            ch = cell & V32TERM_CHAR;
            ink = v32term_palette[(cell >> 12) & 15];
            paper = color_black;
            if (cell & V32TERM_REVERSE) {
                swap = ink;
                ink = paper;
                paper = swap;
            }
            select_region(V32TERM_REG_PAPER);
            set_multiply_color(paper);
            draw_region_zoomed_at(px, py);
            if (ch > ' ' && ch < 127) {
                select_region(V32TERM_REG_BODY + ch);
                set_multiply_color(ink);
                draw_region_zoomed_at(px, py);
                kind = v32term_tail_kind(ch);
                if (kind != 0) {
                    select_region(V32TERM_REG_TAIL + ch);
                    draw_region_zoomed_at(px, py + V32TERM_BODY_ROWS);
                    if (kind == 2) {
                        select_region(V32TERM_REG_TAIL2 + ch);
                        draw_region_zoomed_at(px, py + V32TERM_BODY_ROWS + 1);
                    }
                }
            } else if (ch >= V32TERM_SHADE_FIRST && ch <= V32TERM_SHADE_LAST) {
                select_region(V32TERM_REG_SHADE + (ch - V32TERM_SHADE_FIRST) * 2 +
                              ((i / V32TERM_COLS) & 1));
                set_multiply_color(ink);
                set_drawing_scale(1.0, 1.0);
                draw_region_zoomed_at(px, py);
                set_drawing_scale(0.8, 1.0);
            }
            v32term_shown[i] = cell;
        }
        px += V32TERM_CELL_W;
        if (px >= V32TERM_COLS * V32TERM_CELL_W) {
            px = 0;
            py += V32TERM_CELL_H;
        }
    }
    set_multiply_color(color_white);
    set_drawing_scale(1.0, 1.0);
    v32term_dirty = 0;
}

void v32term_frame(void)
{
    v32term_flush();
    end_frame();
}

/* ---- teletype ------------------------------------------------------------- */

void v32term_goto(int y, int x)
{
    v32term_tty_y = y;
    v32term_tty_x = x;
}

void v32term_attr(int attr)
{
    v32term_tty_attr = attr;
}

static void v32term_scroll(void)
{
    int i;

    for (i = 0; i < V32TERM_CELLS - V32TERM_COLS; i++)
        v32term_want[i] = v32term_want[i + V32TERM_COLS];
    for (i = V32TERM_CELLS - V32TERM_COLS; i < V32TERM_CELLS; i++)
        v32term_want[i] = ' ';
    v32term_dirty_all();
}

void v32term_putc(int c)
{
    if (!v32term_ready)
        v32term_init();
    c &= V32TERM_CHAR;
    if (c == '\n') {
        v32term_tty_x = 0;
        v32term_tty_y++;
    } else if (c == '\r') {
        v32term_tty_x = 0;
    } else if (c == '\b') {
        if (v32term_tty_x > 0)
            v32term_tty_x--;
    } else if (c == '\t') {
        v32term_tty_x = (v32term_tty_x + 8) & ~7;
    } else if (c >= ' ') {
        if (v32term_tty_x >= V32TERM_COLS) {
            v32term_tty_x = 0;
            v32term_tty_y++;
        }
        while (v32term_tty_y >= V32TERM_ROWS) {
            v32term_scroll();
            v32term_tty_y--;
        }
        v32term_set(v32term_tty_y, v32term_tty_x, c | v32term_tty_attr);
        v32term_tty_x++;
    }
    while (v32term_tty_y >= V32TERM_ROWS) {
        v32term_scroll();
        v32term_tty_y--;
    }
}

void v32term_puts(char *s)
{
    while (*s != '\0')
        v32term_putc(*s++);
}

/* ---- pop-up building blocks ------------------------------------------------ */

void v32term_text(int y, int x, char *s, int attr)
{
    while (*s != '\0') {
        v32term_set(y, x, (*s & V32TERM_CHAR) | attr);
        s++;
        x++;
    }
}

void v32term_fill(int y, int x, int h, int w, int cell)
{
    int i, j;

    for (i = 0; i < h; i++)
        for (j = 0; j < w; j++)
            v32term_set(y + i, x + j, cell);
}

void v32term_box(int y, int x, int h, int w, int attr)
{
    int i;

    v32term_fill(y, x, h, w, ' ' | attr);
    for (i = 1; i < w - 1; i++) {
        v32term_set(y, x + i, '-' | attr);
        v32term_set(y + h - 1, x + i, '-' | attr);
    }
    for (i = 1; i < h - 1; i++) {
        v32term_set(y + i, x, '|' | attr);
        v32term_set(y + i, x + w - 1, '|' | attr);
    }
    v32term_set(y, x, '+' | attr);
    v32term_set(y, x + w - 1, '+' | attr);
    v32term_set(y + h - 1, x, '+' | attr);
    v32term_set(y + h - 1, x + w - 1, '+' | attr);
}

void v32term_save(int *buffer)
{
    int i;

    for (i = 0; i < V32TERM_CELLS; i++)
        buffer[i] = v32term_want[i];
}

void v32term_restore(int *buffer)
{
    int i;

    for (i = 0; i < V32TERM_CELLS; i++)
        v32term_want[i] = buffer[i];
    v32term_dirty_all();
}

/* ---- gamepad -------------------------------------------------------------- */

/* For each button: how many polls in a row it has been down (0 = up).
 * Counted in polls, not read from the console's own "frames held"
 * counters, on purpose: a program only polls while it waits for input,
 * and a button pressed while it was busy must still register as a fresh
 * press when it gets back to asking. */
static int v32pad_state[V32PAD_COUNT];

#define V32PAD_REPEAT_DELAY 18              /* polls before a held direction repeats */
#define V32PAD_REPEAT_RATE  5               /* polls between repeats */

static void v32pad_update(int button, int frames_held)
{
    if (frames_held > 0)
        v32pad_state[button]++;
    else
        v32pad_state[button] = 0;
}

void v32pad_poll(void)
{
    v32term_frame();
    select_gamepad(0);
    v32pad_update(V32PAD_LEFT, gamepad_left());
    v32pad_update(V32PAD_RIGHT, gamepad_right());
    v32pad_update(V32PAD_UP, gamepad_up());
    v32pad_update(V32PAD_DOWN, gamepad_down());
    v32pad_update(V32PAD_A, gamepad_button_a());
    v32pad_update(V32PAD_B, gamepad_button_b());
    v32pad_update(V32PAD_X, gamepad_button_x());
    v32pad_update(V32PAD_Y, gamepad_button_y());
    v32pad_update(V32PAD_L, gamepad_button_l());
    v32pad_update(V32PAD_R, gamepad_button_r());
    v32pad_update(V32PAD_START, gamepad_button_start());
}

int v32pad_held_frames(int button)
{
    return v32pad_state[button];
}

int v32pad_down(int button)
{
    return v32pad_state[button] > 0;
}

int v32pad_hit(int button)
{
    int n = v32pad_state[button];

    if (n == 1)
        return 1;
    if (button <= V32PAD_DOWN && n > V32PAD_REPEAT_DELAY &&
        (n - V32PAD_REPEAT_DELAY) % V32PAD_REPEAT_RATE == 0)
        return 1;
    return 0;
}

int v32pad_any_hit(void)
{
    int b;

    for (b = V32PAD_A; b < V32PAD_COUNT; b++)
        if (v32pad_state[b] == 1)
            return 1;
    return 0;
}

/* ---- on-screen keyboard ---------------------------------------------------- */

#define V32OSK_ROWS 4
#define V32OSK_KEYS 12

static char *v32osk_plain[V32OSK_ROWS] = {
    "1234567890-=",
    "qwertyuiop[]",
    "asdfghjkl;'\\",
    "zxcvbnm,./`*"
};
static char *v32osk_shift[V32OSK_ROWS] = {
    "!@#$%^&*()_+",
    "QWERTYUIOP{}",
    "ASDFGHJKL:\"|",
    "ZXCVBNM<>?~*"
};

static int v32osk_row = 1;
static int v32osk_col = 0;
static int v32osk_shifted = 0;
static int v32osk_saved[V32TERM_CELLS];

#define V32OSK_W 42
#define V32OSK_H 9

static void v32osk_draw(int top, int left, char *title, int ctrl)
{
    int r, k, attr;
    char *keys;
    int panel = V32TERM_INK(V32INK_CYAN);

    v32term_box(top, left, V32OSK_H, V32OSK_W, panel);
    if (title != NULL)
        v32term_text(top, left + 2, title, V32TERM_INK(V32INK_YELLOW));
    for (r = 0; r < V32OSK_ROWS; r++) {
        keys = v32osk_shifted ? v32osk_shift[r] : v32osk_plain[r];
        for (k = 0; k < V32OSK_KEYS; k++) {
            attr = V32TERM_INK(V32INK_WHITE);
            if (r == v32osk_row && k == v32osk_col)
                attr = V32TERM_REVERSE | V32TERM_INK(V32INK_YELLOW);
            v32term_set(top + 1 + r, left + 3 + k * 3, ' ' | attr);
            v32term_set(top + 1 + r, left + 4 + k * 3, (keys[k] & V32TERM_CHAR) | attr);
            v32term_set(top + 1 + r, left + 5 + k * 3, ' ' | attr);
        }
    }
    v32term_text(top + 5, left + 2, "A type  B Esc  X Bksp  Y Space", V32TERM_INK(V32INK_GRAY));
    v32term_text(top + 6, left + 2, "L Shift       R Ctrl   START Enter", V32TERM_INK(V32INK_GRAY));
    if (v32osk_shifted)
        v32term_text(top + 6, left + 2, "L Shift", V32TERM_REVERSE | V32TERM_INK(V32INK_GREEN));
    if (ctrl)
        v32term_text(top + 6, left + 16, "R Ctrl", V32TERM_REVERSE | V32TERM_INK(V32INK_GREEN));
    v32term_text(top + 7, left + 2, "(* in the last row is the list key)", V32TERM_INK(V32INK_DIM));
}

int v32osk_getkey_titled(char *title)
{
    int top, left, key, ctrl;
    char *keys;

    v32term_init();
    v32term_save(v32osk_saved);
    /* stay out of the way of the line being typed on */
    top = V32TERM_ROWS - V32OSK_H;
    if (v32term_tty_y >= top - 1)
        top = 1;
    left = (V32TERM_COLS - V32OSK_W) / 2;
    key = -1;
    ctrl = 0;
    while (key < 0) {
        v32osk_draw(top, left, title, ctrl);
        v32pad_poll();
        if (v32pad_hit(V32PAD_LEFT))
            v32osk_col = (v32osk_col + V32OSK_KEYS - 1) % V32OSK_KEYS;
        if (v32pad_hit(V32PAD_RIGHT))
            v32osk_col = (v32osk_col + 1) % V32OSK_KEYS;
        if (v32pad_hit(V32PAD_UP))
            v32osk_row = (v32osk_row + V32OSK_ROWS - 1) % V32OSK_ROWS;
        if (v32pad_hit(V32PAD_DOWN))
            v32osk_row = (v32osk_row + 1) % V32OSK_ROWS;
        if (v32pad_hit(V32PAD_L))
            v32osk_shifted = !v32osk_shifted;
        if (v32pad_hit(V32PAD_R))
            ctrl = !ctrl;
        if (v32pad_hit(V32PAD_A)) {
            keys = v32osk_shifted ? v32osk_shift[v32osk_row] : v32osk_plain[v32osk_row];
            key = keys[v32osk_col];
            if (ctrl)
                key &= 31;
        }
        if (v32pad_hit(V32PAD_B))
            key = V32KEY_ESC;
        if (v32pad_hit(V32PAD_X))
            key = V32KEY_BACKSPACE;
        if (v32pad_hit(V32PAD_Y))
            key = ' ';
        if (v32pad_hit(V32PAD_START))
            key = V32KEY_ENTER;
    }
    v32term_restore(v32osk_saved);
    v32term_flush();
    return key;
}

int v32osk_getkey(void)
{
    return v32osk_getkey_titled(NULL);
}

int (*v32term_key_source)(void) = v32osk_getkey;

int v32term_getkey(void)
{
    v32term_init();
    v32term_flush();
    return v32term_key_source();
}

#endif /* V32TERM_C */
