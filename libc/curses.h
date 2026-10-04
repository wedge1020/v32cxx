/* ****************************************************************************
 *  libc/curses.h -- curses for the Vircon32 console, through v32c++.
 *
 *  The classic (4.3BSD / System V) curses a terminal program is written
 *  against: windows of characters, a cursor in each, and wrefresh() to
 *  make the screen match. The screen is v32term's 80x24 grid of BIOS-font
 *  characters (see v32term.h), and keys come from the gamepad through
 *  v32term_key_source (the on-screen keyboard unless the program installs
 *  its own mapping).
 *
 *  Declarations only; the code is in v32curses.c, which the program's one
 *  translation unit #includes once, after <v32libc.c>.
 *
 *  What is here: initscr endwin isendwin, newwin subwin delwin mvwin,
 *  move/addch/addstr/printw/inch/clear/erase/clrtoeol/clrtobot/standout/
 *  standend/refresh/getch/getnstr and their w-, mv- and mvw- forms, box,
 *  touchwin, clearok, getyx/getmaxyx, unctrl, and the terminal-mode calls
 *  (raw, noecho, keypad, ...), which have nothing to do on a console and
 *  do nothing.
 *
 *  What is different from a real terminal:
 *    - a chtype is the character plus A_STANDOUT (reverse video) and an
 *      optional colour, A_INK(n) with a V32INK_ index from v32term.h;
 *    - curses_ink_hook, when set, picks the colour of every character a
 *      window refresh puts on screen -- so an old monochrome program can
 *      be given colour without touching its drawing code.
 * ****************************************************************************/
#ifndef V32_CURSES_H
#define V32_CURSES_H

#include "v32libc.h"
#include "v32term.h"

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define ERR (-1)
#define OK  0

typedef int chtype;

#define A_CHARTEXT  V32TERM_CHAR
#define A_STANDOUT  V32TERM_REVERSE
#define A_REVERSE   V32TERM_REVERSE
#define A_BOLD      0
#define A_NORMAL    0
#define A_ATTRIBUTES (V32TERM_REVERSE | V32TERM_COLOR)
#define A_INK(n)    V32TERM_INK(n)

#define KEY_BACKSPACE 8
#define KEY_ENTER     '\n'

typedef struct _win_st {
    int _cury, _curx;       /* cursor */
    int _maxy, _maxx;       /* size */
    int _begy, _begx;       /* where on the screen */
    int _attrs;             /* attributes for characters added from now on */
    int _clear;             /* clearok(): repaint everything at the next refresh */
    int _stride;            /* cells per row in _cells */
    int _owner;             /* does this window own _cells? (a subwin does not) */
    chtype *_cells;         /* row y starts at _cells + y * _stride */
    int *_dirty;            /* one flag per row: changed since the last refresh */
} WINDOW;

extern WINDOW *stdscr;
extern WINDOW *curscr;
extern int LINES;
extern int COLS;

/* The window whose refresh last changed the screen: what the player is
 * looking at. (Not part of curses; for code that needs to know whether a
 * pop-up window is up.) */
extern WINDOW *curses_shown;

/* The colour of a character about to go on screen from window `win` at
 * (y, x): return a V32INK_ index, or -1 to keep the cell's own. */
extern int (*curses_ink_hook)(WINDOW *win, int y, int x, int ch);

WINDOW *initscr(void);
int endwin(void);
int isendwin(void);

WINDOW *newwin(int lines, int cols, int begin_y, int begin_x);
WINDOW *subwin(WINDOW *parent, int lines, int cols, int begin_y, int begin_x);
int delwin(WINDOW *win);
int mvwin(WINDOW *win, int y, int x);

int wmove(WINDOW *win, int y, int x);
int waddch(WINDOW *win, chtype ch);
int waddstr(WINDOW *win, char *s);
int wprintw(WINDOW *win, char *fmt, ...);
int mvwprintw(WINDOW *win, int y, int x, char *fmt, ...);
int printw(char *fmt, ...);
int mvprintw(int y, int x, char *fmt, ...);
chtype winch(WINDOW *win);
int wclear(WINDOW *win);
int werase(WINDOW *win);
int wclrtoeol(WINDOW *win);
int wclrtobot(WINDOW *win);
int wstandout(WINDOW *win);
int wstandend(WINDOW *win);
int wattron(WINDOW *win, int attrs);
int wattroff(WINDOW *win, int attrs);
int wrefresh(WINDOW *win);
int touchwin(WINDOW *win);
int clearok(WINDOW *win, int flag);
int box(WINDOW *win, chtype vertical, chtype horizontal);
int wgetch(WINDOW *win);
int wgetnstr(WINDOW *win, char *buf, int n);

int mvwaddch(WINDOW *win, int y, int x, chtype ch);
int mvwaddstr(WINDOW *win, int y, int x, char *s);
chtype mvwinch(WINDOW *win, int y, int x);

char *unctrl(chtype ch);
int mvcur(int old_y, int old_x, int new_y, int new_x);
int beep(void);
int flushinp(void);
int baudrate(void);
int erasechar(void);
int killchar(void);

/* stdscr forms */
#define move(y, x)            wmove(stdscr, y, x)
#define addch(ch)             waddch(stdscr, ch)
#define addstr(s)             waddstr(stdscr, s)
#define inch()                winch(stdscr)
#define clear()               wclear(stdscr)
#define erase()               werase(stdscr)
#define clrtoeol()            wclrtoeol(stdscr)
#define clrtobot()            wclrtobot(stdscr)
#define standout()            wstandout(stdscr)
#define standend()            wstandend(stdscr)
#define attron(a)             wattron(stdscr, a)
#define attroff(a)            wattroff(stdscr, a)
#define refresh()             wrefresh(stdscr)
#define getch()               wgetch(stdscr)
#define getstr(buf)           wgetnstr(stdscr, buf, 1024)
#define getnstr(buf, n)       wgetnstr(stdscr, buf, n)
#define wgetstr(win, buf)     wgetnstr(win, buf, 1024)
#define mvaddch(y, x, ch)     mvwaddch(stdscr, y, x, ch)
#define mvaddstr(y, x, s)     mvwaddstr(stdscr, y, x, s)
#define mvinch(y, x)          mvwinch(stdscr, y, x)

#define getyx(win, y, x)      (y = (win)->_cury, x = (win)->_curx)
#define getmaxyx(win, y, x)   (y = (win)->_maxy, x = (win)->_maxx)
#define getmaxy(win)          ((win)->_maxy)
#define getmaxx(win)          ((win)->_maxx)
#define getbegyx(win, y, x)   (y = (win)->_begy, x = (win)->_begx)

/* Terminal modes: nothing to switch on a console. */
#define raw()                 (OK)
#define noraw()               (OK)
#define cbreak()              (OK)
#define nocbreak()            (OK)
#define crmode()              (OK)
#define nocrmode()            (OK)
#define echo()                (OK)
#define noecho()              (OK)
#define nl()                  (OK)
#define nonl()                (OK)
#define keypad(win, flag)     (OK)
#define idlok(win, flag)      (OK)
#define leaveok(win, flag)    (OK)
#define scrollok(win, flag)   (OK)
#define nodelay(win, flag)    (OK)
#define typeahead(fd)         (OK)
#define halfdelay(tenths)     (OK)

#endif /* V32_CURSES_H */
