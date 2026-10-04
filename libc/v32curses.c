/* ****************************************************************************
 *  libc/v32curses.c -- see curses.h. #include this once in the program's
 *  one translation unit, after <v32libc.c>.
 * ****************************************************************************/
#ifndef V32_CURSES_C
#define V32_CURSES_C

#include "curses.h"

WINDOW *stdscr = NULL;
WINDOW *curscr = NULL;
int LINES = V32TERM_ROWS;
int COLS = V32TERM_COLS;

int (*curses_ink_hook)(WINDOW *win, int y, int x, int ch) = NULL;
WINDOW *curses_shown = NULL;

static int curses_ended = 1;
static int curses_clear_next = 0;       /* clearok(curscr): wipe the screen at the next refresh */

static void curses_touch_all(WINDOW *win)
{
    int y;

    for (y = 0; y < win->_maxy; y++)
        win->_dirty[y] = 1;
}

static void curses_blank(WINDOW *win)
{
    int y, x;

    for (y = 0; y < win->_maxy; y++) {
        for (x = 0; x < win->_maxx; x++)
            win->_cells[y * win->_stride + x] = ' ';
        win->_dirty[y] = 1;
    }
}

WINDOW *newwin(int lines, int cols, int begin_y, int begin_x)
{
    WINDOW *win;

    if (lines <= 0)
        lines = LINES - begin_y;
    if (cols <= 0)
        cols = COLS - begin_x;
    win = (WINDOW *) malloc(sizeof(WINDOW));
    if (win == NULL)
        return NULL;
    win->_cury = 0;
    win->_curx = 0;
    win->_maxy = lines;
    win->_maxx = cols;
    win->_begy = begin_y;
    win->_begx = begin_x;
    win->_attrs = 0;
    win->_clear = 0;
    win->_stride = cols;
    win->_owner = 1;
    win->_cells = (chtype *) malloc(lines * cols * sizeof(chtype));
    win->_dirty = (int *) malloc(lines * sizeof(int));
    curses_blank(win);
    return win;
}

/* A window onto part of `parent`: it shares the parent's characters. */
WINDOW *subwin(WINDOW *parent, int lines, int cols, int begin_y, int begin_x)
{
    WINDOW *win;
    int off_y = begin_y - parent->_begy;
    int off_x = begin_x - parent->_begx;

    if (off_y < 0 || off_x < 0 || off_y + lines > parent->_maxy || off_x + cols > parent->_maxx)
        return NULL;
    win = (WINDOW *) malloc(sizeof(WINDOW));
    if (win == NULL)
        return NULL;
    win->_cury = 0;
    win->_curx = 0;
    win->_maxy = lines;
    win->_maxx = cols;
    win->_begy = begin_y;
    win->_begx = begin_x;
    win->_attrs = 0;
    win->_clear = 0;
    win->_stride = parent->_stride;
    win->_owner = 0;
    win->_cells = parent->_cells + off_y * parent->_stride + off_x;
    win->_dirty = parent->_dirty + off_y;
    return win;
}

int delwin(WINDOW *win)
{
    if (win == NULL)
        return ERR;
    if (win == curscr) {
        /* curscr is the screen itself, not storage of its own */
        curscr = NULL;
        free(win);
        return OK;
    }
    if (win->_owner) {
        free(win->_cells);
        free(win->_dirty);
    }
    if (win == stdscr)
        stdscr = NULL;
    free(win);
    return OK;
}

int mvwin(WINDOW *win, int y, int x)
{
    if (y < 0 || x < 0 || y + win->_maxy > LINES || x + win->_maxx > COLS)
        return ERR;
    win->_begy = y;
    win->_begx = x;
    curses_touch_all(win);
    return OK;
}

WINDOW *initscr(void)
{
    v32term_init();
    v32term_clear();
    if (stdscr == NULL)
        stdscr = newwin(LINES, COLS, 0, 0);
    if (curscr == NULL) {
        /* curscr stands for "what is on the screen": only its cursor and
         * its clearok() flag are ever used */
        curscr = (WINDOW *) malloc(sizeof(WINDOW));
        curscr->_cury = 0;
        curscr->_curx = 0;
        curscr->_maxy = LINES;
        curscr->_maxx = COLS;
        curscr->_begy = 0;
        curscr->_begx = 0;
        curscr->_attrs = 0;
        curscr->_clear = 0;
        curscr->_stride = COLS;
        curscr->_owner = 0;
        curscr->_cells = NULL;
        curscr->_dirty = NULL;
    }
    curses_ended = 0;
    curses_clear_next = 1;
    return stdscr;
}

int endwin(void)
{
    curses_ended = 1;
    /* like a terminal leaving curses mode: the picture stays, and plain
     * output continues from the bottom line */
    v32term_goto(V32TERM_ROWS - 1, 0);
    v32term_attr(0);
    v32term_flush();
    return OK;
}

int isendwin(void)
{
    return curses_ended;
}

int wmove(WINDOW *win, int y, int x)
{
    if (y < 0 || y >= win->_maxy || x < 0 || x >= win->_maxx)
        return ERR;
    win->_cury = y;
    win->_curx = x;
    return OK;
}

static void curses_put(WINDOW *win, int y, int x, chtype cell)
{
    chtype *p = win->_cells + y * win->_stride + x;

    if (*p != cell) {
        *p = cell;
        win->_dirty[y] = 1;
    }
}

int waddch(WINDOW *win, chtype ch)
{
    int c = ch & A_CHARTEXT;
    int attrs = (ch & A_ATTRIBUTES) | win->_attrs;
    int y = win->_cury;
    int x = win->_curx;

    if (c == '\n') {
        wclrtoeol(win);
        x = 0;
        y++;
    } else if (c == '\r') {
        x = 0;
    } else if (c == '\b') {
        if (x > 0)
            x--;
    } else if (c == '\t') {
        do {
            curses_put(win, y, x, ' ' | attrs);
            x++;
        } while (x < win->_maxx && (x & 7) != 0);
    } else {
        if (c < ' ') {
            /* a control character shows as ^X, as curses does */
            curses_put(win, y, x, '^' | attrs);
            x++;
            if (x >= win->_maxx) {
                x = 0;
                y++;
            }
            c += '@';
        }
        if (y < win->_maxy) {
            curses_put(win, y, x, c | attrs);
            x++;
        }
    }
    if (x >= win->_maxx) {
        x = 0;
        y++;
    }
    if (y >= win->_maxy) {
        /* no scrolling: stay on the last cell */
        y = win->_maxy - 1;
        x = win->_maxx - 1;
        win->_cury = y;
        win->_curx = x;
        return ERR;
    }
    win->_cury = y;
    win->_curx = x;
    return OK;
}

int waddstr(WINDOW *win, char *s)
{
    while (*s != '\0') {
        if (waddch(win, (chtype)(*s & A_CHARTEXT)) == ERR)
            return ERR;
        s++;
    }
    return OK;
}

static char curses_printw_buf[2048];

int wprintw(WINDOW *win, char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsprintf(curses_printw_buf, fmt, args);
    va_end(args);
    return waddstr(win, curses_printw_buf);
}

int mvwprintw(WINDOW *win, int y, int x, char *fmt, ...)
{
    va_list args;

    if (wmove(win, y, x) == ERR)
        return ERR;
    va_start(args, fmt);
    vsprintf(curses_printw_buf, fmt, args);
    va_end(args);
    return waddstr(win, curses_printw_buf);
}

int printw(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsprintf(curses_printw_buf, fmt, args);
    va_end(args);
    return waddstr(stdscr, curses_printw_buf);
}

int mvprintw(int y, int x, char *fmt, ...)
{
    va_list args;

    if (wmove(stdscr, y, x) == ERR)
        return ERR;
    va_start(args, fmt);
    vsprintf(curses_printw_buf, fmt, args);
    va_end(args);
    return waddstr(stdscr, curses_printw_buf);
}

chtype winch(WINDOW *win)
{
    return win->_cells[win->_cury * win->_stride + win->_curx];
}

int werase(WINDOW *win)
{
    curses_blank(win);
    win->_cury = 0;
    win->_curx = 0;
    return OK;
}

int wclear(WINDOW *win)
{
    werase(win);
    win->_clear = 1;
    return OK;
}

int wclrtoeol(WINDOW *win)
{
    int x;

    for (x = win->_curx; x < win->_maxx; x++)
        curses_put(win, win->_cury, x, ' ');
    return OK;
}

int wclrtobot(WINDOW *win)
{
    int y, x;

    wclrtoeol(win);
    for (y = win->_cury + 1; y < win->_maxy; y++)
        for (x = 0; x < win->_maxx; x++)
            curses_put(win, y, x, ' ');
    return OK;
}

int wstandout(WINDOW *win)
{
    win->_attrs |= A_STANDOUT;
    return OK;
}

int wstandend(WINDOW *win)
{
    win->_attrs = 0;
    return OK;
}

int wattron(WINDOW *win, int attrs)
{
    if (attrs & V32TERM_COLOR)
        win->_attrs &= ~V32TERM_COLOR;
    win->_attrs |= attrs;
    return OK;
}

int wattroff(WINDOW *win, int attrs)
{
    win->_attrs &= ~attrs;
    return OK;
}

int touchwin(WINDOW *win)
{
    if (win != NULL && win != curscr)
        curses_touch_all(win);
    return OK;
}

int clearok(WINDOW *win, int flag)
{
    if (win == curscr)
        curses_clear_next = flag;
    else
        win->_clear = flag;
    return OK;
}

/* Makes the screen match the window. Only rows changed since the last
 * refresh are copied -- which is why a program must touchwin() a window
 * that another one was drawn over, exactly as on a real terminal. */
int wrefresh(WINDOW *win)
{
    int y, x, cell, ink;
    chtype *row;

    if (win == NULL)
        return ERR;
    curses_ended = 0;
    if (win == curscr) {
        /* "redraw the screen as curses believes it is" */
        v32term_redraw();
        v32term_flush();
        return OK;
    }
    if (curses_clear_next || win->_clear) {
        if (curses_clear_next || (win->_maxy == LINES && win->_maxx == COLS))
            v32term_clear();
        curses_clear_next = 0;
        win->_clear = 0;
        curses_touch_all(win);
        v32term_redraw();
    }
    for (y = 0; y < win->_maxy; y++) {
        if (!win->_dirty[y])
            continue;
        win->_dirty[y] = 0;
        curses_shown = win;
        row = win->_cells + y * win->_stride;
        for (x = 0; x < win->_maxx; x++) {
            cell = row[x];
            if (curses_ink_hook != NULL) {
                ink = curses_ink_hook(win, y, x, cell & A_CHARTEXT);
                if (ink >= 0)
                    cell = (cell & ~V32TERM_COLOR) | V32TERM_INK(ink);
            }
            v32term_set(win->_begy + y, win->_begx + x, cell);
        }
    }
    if (curscr != NULL) {
        curscr->_cury = win->_begy + win->_cury;
        curscr->_curx = win->_begx + win->_curx;
    }
    v32term_flush();
    return OK;
}

int box(WINDOW *win, chtype vertical, chtype horizontal)
{
    int y, x;

    if (vertical == 0)
        vertical = '|';
    if (horizontal == 0)
        horizontal = '-';
    for (x = 0; x < win->_maxx; x++) {
        curses_put(win, 0, x, horizontal);
        curses_put(win, win->_maxy - 1, x, horizontal);
    }
    for (y = 0; y < win->_maxy; y++) {
        curses_put(win, y, 0, vertical);
        curses_put(win, y, win->_maxx - 1, vertical);
    }
    return OK;
}

int mvwaddch(WINDOW *win, int y, int x, chtype ch)
{
    if (wmove(win, y, x) == ERR)
        return ERR;
    return waddch(win, ch);
}

int mvwaddstr(WINDOW *win, int y, int x, char *s)
{
    if (wmove(win, y, x) == ERR)
        return ERR;
    return waddstr(win, s);
}

chtype mvwinch(WINDOW *win, int y, int x)
{
    if (wmove(win, y, x) == ERR)
        return (chtype) ERR;
    return winch(win);
}

int wgetch(WINDOW *win)
{
    wrefresh(win);
    return v32term_getkey();
}

/* Reads a line into buf, echoing it in the window. Backspace edits. */
int wgetnstr(WINDOW *win, char *buf, int n)
{
    int len = 0;
    int c;

    for (;;) {
        c = wgetch(win);
        if (c == '\n' || c == '\r' || c == V32KEY_ESC)
            break;
        if (c == KEY_BACKSPACE || c == 127) {
            if (len > 0 && win->_curx > 0) {
                len--;
                win->_curx--;
                curses_put(win, win->_cury, win->_curx, ' ');
            }
            continue;
        }
        if (len < n - 1 && c >= ' ' && c < 127) {
            buf[len++] = (char)c;
            waddch(win, (chtype)c);
        }
    }
    buf[len] = '\0';
    wrefresh(win);
    return OK;
}

static char curses_unctrl_buf[4];

char *unctrl(chtype ch)
{
    int c = ch & A_CHARTEXT;

    if (c < ' ') {
        curses_unctrl_buf[0] = '^';
        curses_unctrl_buf[1] = (char)(c + '@');
        curses_unctrl_buf[2] = '\0';
    } else if (c == 127) {
        curses_unctrl_buf[0] = '^';
        curses_unctrl_buf[1] = '?';
        curses_unctrl_buf[2] = '\0';
    } else {
        curses_unctrl_buf[0] = (char)c;
        curses_unctrl_buf[1] = '\0';
    }
    return curses_unctrl_buf;
}

int mvcur(int old_y, int old_x, int new_y, int new_x)
{
    old_y = old_y;
    old_x = old_x;
    v32term_goto(new_y, new_x);
    return OK;
}

int beep(void)
{
    return OK;
}

int flushinp(void)
{
    return OK;
}

int baudrate(void)
{
    return 38400;
}

int erasechar(void)
{
    return KEY_BACKSPACE;
}

int killchar(void)
{
    return 21;      /* ^U */
}

#endif /* V32_CURSES_C */
