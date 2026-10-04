/* ****************************************************************************
 *  libc/v32term.h -- an 80x24 character terminal on the Vircon32 screen.
 *
 *  The console's screen is 640x360 and the BIOS font is 10x20, which gives
 *  only 64x18 characters. A character cell here is 8x15 instead, so 80
 *  columns by 24 rows fill the screen exactly -- the size every terminal
 *  program assumes.
 *
 *  Texture -1 is the BIOS texture; its region N is the glyph for character
 *  N, and region 20 is a solid block. A cell is painted as the solid block
 *  in the background colour, then the glyph in the foreground colour. The
 *  glyphs are fitted into the smaller cell without losing a stroke; how is
 *  described in v32term.c.
 *
 *  The screen is not redrawn every frame: the terminal keeps what it last
 *  drew and repaints only the cells that changed (the Vircon32 frame
 *  buffer keeps its contents between frames).
 *
 *  Used by v32libc.c (printf, getchar) and by curses.h. Code is in
 *  v32term.c, included by v32libc.c.
 * ****************************************************************************/
#ifndef V32TERM_H
#define V32TERM_H

#define V32TERM_COLS   80
#define V32TERM_ROWS   24
#define V32TERM_CELLS  (V32TERM_COLS * V32TERM_ROWS)
#define V32TERM_CELL_W 8
#define V32TERM_CELL_H 15

/* A cell is one word: the character, plus attribute bits. */
#define V32TERM_CHAR    0x00FF
#define V32TERM_REVERSE 0x0100              /* black on colour */
#define V32TERM_COLOR   0xF000              /* palette index, see below */
#define V32TERM_INK(n)  ((n) << 12)

/* Characters 17..20 are the BIOS font's shade blocks: a quarter, a half
 * and three quarters of the pixels lit, and a solid block. */
#define V32TERM_SHADE_FIRST 17
#define V32TERM_SHADE_LIGHT 17
#define V32TERM_SHADE_HALF  18
#define V32TERM_SHADE_DARK  19
#define V32TERM_SHADE_SOLID 20
#define V32TERM_SHADE_LAST  20

/* Palette indices for V32TERM_INK(). 0 is the default text colour. */
#define V32INK_DEFAULT  0
#define V32INK_RED      1
#define V32INK_GREEN    2
#define V32INK_YELLOW   3
#define V32INK_BLUE     4
#define V32INK_MAGENTA  5
#define V32INK_CYAN     6
#define V32INK_WHITE    7
#define V32INK_GRAY     8
#define V32INK_ORANGE   9
#define V32INK_BROWN    10
#define V32INK_DIM      11
#define V32INK_DKGREEN  12

/* Key codes v32term_getkey() can return besides plain characters. */
#define V32KEY_ESC       27
#define V32KEY_ENTER     '\n'
#define V32KEY_BACKSPACE 8

void v32term_init(void);                    /* idempotent */
void v32term_clear(void);                   /* blank every cell */
void v32term_set(int y, int x, int cell);
int  v32term_get(int y, int x);
int  v32term_find(char *text, int first_row, int last_row);
                                            /* is this text on those rows? (1 or 0) */
void v32term_flush(void);                   /* draw the cells that changed */
void v32term_redraw(void);                  /* forget what is drawn: next flush paints it all */
void v32term_frame(void);                   /* flush, then wait for the next frame */

/* Teletype output: a cursor, line wrap, scrolling at the bottom row. */
void v32term_goto(int y, int x);
void v32term_attr(int attr);                /* V32TERM_REVERSE | V32TERM_INK(n) for what follows */

/* Text and boxes straight onto the screen, and saving what is under them:
 * what pop-up menus are made of. */
void v32term_text(int y, int x, char *s, int attr);
void v32term_fill(int y, int x, int h, int w, int cell);
void v32term_box(int y, int x, int h, int w, int attr);
void v32term_save(int *buffer);             /* V32TERM_CELLS words */
void v32term_restore(int *buffer);

/* ---- gamepad ------------------------------------------------------------
 * v32pad_poll() ends the frame (after flushing the screen) and reads
 * gamepad 0. v32pad_hit(b) is then true on the first poll that finds
 * button b down, and -- for the four directions -- again at a steady rate
 * while it stays down. v32pad_down(b) is true the whole time b is held. */
#define V32PAD_LEFT   0
#define V32PAD_RIGHT  1
#define V32PAD_UP     2
#define V32PAD_DOWN   3
#define V32PAD_A      4
#define V32PAD_B      5
#define V32PAD_X      6
#define V32PAD_Y      7
#define V32PAD_L      8
#define V32PAD_R      9
#define V32PAD_START  10
#define V32PAD_COUNT  11

void v32pad_poll(void);
int  v32pad_hit(int button);
int  v32pad_down(int button);
int  v32pad_held_frames(int button);        /* polls in a row it has been down; 0 when up */
int  v32pad_any_hit(void);

/* ---- keyboard -----------------------------------------------------------
 * A program that reads keys gets them from v32term_key_source. The
 * default is the on-screen keyboard: every key read pops it up. A program
 * with its own idea of how a gamepad should type (a game mapping buttons
 * to its commands) points v32term_key_source at its own function, and can
 * still call v32osk_getkey() for the cases it does not cover. */
extern int (*v32term_key_source)(void);

/* The on-screen keyboard. Returns one character:
 *   D-pad  move        A  type the highlighted key
 *   B      Escape      X  Backspace      Y  Space
 *   L      Shift       R  Ctrl (for the next key)
 *   START  Enter
 * `title` (may be NULL) is shown on its top border. */
int v32osk_getkey(void);
int v32osk_getkey_titled(char *title);

#endif /* V32TERM_H */
