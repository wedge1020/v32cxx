/* *****************************************************************************
 *  tests/116sample.c -- old C, as programs written for Unix in the 1980s use
 *  it, transpiled as C (the .c extension selects C input). Every item here
 *  was found by demos/c/rogue, the BSD game Rogue 5.4.4:
 *
 *   1. variadic functions: `...`, va_list, va_start, va_arg, va_end; a
 *      va_list handed on to another function; no extra arguments at all; a
 *      variadic call inside an argument and inside a condition;
 *   2. pointers to pointers: `char **argv`, `char **a, *b;`, `(char **)`;
 *   3. C's separate tag namespace: `struct rdes { ... } rdes[3];`, a tag
 *      used before its definition, a struct defined where a variable is
 *      declared, anonymous structs inside a union, `typedef struct { } T;`;
 *   4. declarations that say less than the definition: `void fatal();`,
 *      `void leave(int);`, a function and an `extern` array declared
 *      inside a function body, `extern char names[];` ahead of the array;
 *   5. function pointers the K&R way: `void (*d_func)();` called with an
 *      argument, `void (*func)()` as a parameter, `(void (*)())fn`, a
 *      function's name in an initializer table and in a comparison;
 *   6. the null pointer is 0: an uninitialized static pointer, zeroed
 *      memory, `if (p)`, `!p`, `p && p->x`, `while (p)`;
 *   7. sizeof in a constant expression: an array sized by `sizeof table /
 *      sizeof (struct ...)`, by `sizeof "text"`;
 *   8. octal: `#define ISHELD 0000400` is 256, `c & 037`;
 *   9. main(argc, argv, envp);
 *  10. `(void) f();`, `auto`, `register`, and variables called `this`,
 *      `new` and `class`;
 *  11. a string with a control character in it; a string initializing a
 *      char array in the middle of a struct.
 *
 *  Self-checking: test_errors must end at 0. Also valid for a native C
 *  compiler (gcc -x c tests/116sample.c && ./a.out prints the count).
 * ****************************************************************************/

#include <stdarg.h>
#ifndef __V32CXX__
#include <stdio.h>
#include <string.h>
#endif

int test_errors = -1;

/* ---- 4: declarations that say less than the definition -------------------- */
void fatal();                       /* K&R: parameters unspecified */
void leave(int);                    /* unnamed parameter */
int  count_args(char *fmt, ...);
extern char names[];                /* defined further down */
extern int seed;

/* ---- 8: octal --------------------------------------------------------------- */
#define ISHELD  0000400
#define ISRUN   0000004
#define CTRL(c) (c & 037)

/* ---- 3: tags and typedefs --------------------------------------------------- */
typedef struct {
    int x;
    int y;
} coord;

struct stats {
    int  s_str;
    char s_dmg[13];
    int  s_maxhp;
};

union thing {
    struct {
        union thing *_l_next, *_l_prev;
        coord _t_pos;
        char *_t_name;
    } _t;
    struct {
        union thing *_l_next, *_l_prev;
        int _o_type;
        int _o_count;
        char *_o_label;
    } _o;
};
typedef union thing THING;

#define l_next  _t._l_next
#define t_pos   _t._t_pos
#define o_count _o._o_count
#define o_label _o._o_label
#define next(ptr) (*ptr).l_next

struct delayed_action {
    int d_type;
    void (*d_func)();
    int d_arg;
} d_list[4];

/* ---- 11: a string inside a struct initializer ------------------------------- */
struct stats max_stats = { 16, "1x4", 12 };

/* ---- 7: sizeof in constant expressions -------------------------------------- */
char *rainbow[] = { "amber", "aquamarine", "black", "blue", "brown" };
#define NCOLORS (sizeof rainbow / sizeof (char *))
coord corners[] = { { 0, 0 }, { 79, 0 }, { 0, 23 } };
#define NCORNERS (sizeof corners / sizeof (coord))
#define MAX2(a, b) (a > b ? a : b)
int used[MAX2(NCOLORS, NCORNERS)];
#define MAXMSG (80 - sizeof "--More--")
char msgbuf[2 * MAXMSG + 1];

char names[] = "abc";
int seed = 7;

/* ---- 6: pointers nobody initialized ----------------------------------------- */
THING *pack;
static char *last_message;

int fatal_calls = 0;
int leave_calls = 0;
int daemon_total = 0;

void
fatal(char *s)
{
    if (s[0] == 'x')
        fatal_calls++;
}

void
leave(int sig)
{
    leave_calls += sig;
}

/* ---- 1: variadic functions -------------------------------------------------- */
int
sum_ints(int n, va_list args)
{
    int total = 0;

    while (n-- > 0)
        total += va_arg(args, int);
    return total;
}

int
sum(int n, ...)
{
    va_list args;
    int total;

    va_start(args, n);
    total = sum_ints(n, args);
    va_end(args);
    return total;
}

/* counts the %-conversions it is given arguments for, checking each */
int
count_args(char *fmt, ...)
{
    va_list ap;
    int count = 0;
    char *s;

    va_start(ap, fmt);
    for (; *fmt != '\0'; fmt++)
    {
        if (*fmt != '%')
            continue;
        fmt++;
        if (*fmt == 'd')
        {
            if (va_arg(ap, int) == 42)
                count++;
        }
        else if (*fmt == 's')
        {
            s = va_arg(ap, char *);
            if (s != NULL && s[0] == 'o' && s[1] == 'k')
                count++;
        }
        else if (*fmt == 'c')
        {
            if (va_arg(ap, int) == 'z')
                count++;
        }
    }
    va_end(ap);
    return count;
}

/* ---- 5: daemons, the way Rogue runs them ------------------------------------ */
void
doctor()
{
    daemon_total += 1;
}

void
turn_see(int turn_off)
{
    daemon_total += turn_off ? 100 : 10;
}

void
start_daemon(void (*func)(), int arg, int slot)
{
    d_list[slot].d_type = 1;
    d_list[slot].d_func = func;
    d_list[slot].d_arg = arg;
}

struct delayed_action *
find_slot(void (*func)())
{
    struct delayed_action *dev;

    for (dev = d_list; dev <= &d_list[3]; dev++)
        if (dev->d_type != 0 && func == dev->d_func)
            return dev;
    return NULL;
}

void
do_daemons()
{
    struct delayed_action *dev;

    for (dev = d_list; dev <= &d_list[3]; dev++)
        if (dev->d_type != 0)
            (*dev->d_func)(dev->d_arg);
}

struct h_list {
    char h_ch;
    void (*h_func)();
};

struct h_list handlers[] = {
    { 'd', doctor },
    { 0, 0 }
};

/* ---- 2: pointers to pointers ------------------------------------------------ */
int
count_strings(char **list)
{
    char **p, *first;
    int n = 0;

    first = *list;
    for (p = list; *p != NULL; p++)
        n++;
    return first == list[0] ? n : -1;
}

void
attach(THING **list, THING *item)
{
    next(item) = *list;
    *list = item;
}

/* ---- 3: a tag used before, and named like, a variable ----------------------- */
int
rooms_connected()
{
    struct rdes *r1;
    int total = 0;
    static struct rdes
    {
        int conn[3];
        int ingraph;
    } rdes[3] = {
        { { 0, 1, 0 }, 0 },
        { { 1, 0, 1 }, 0 },
        { { 0, 1, 0 }, 1 },
    };

    for (r1 = rdes; r1 <= &rdes[2]; r1++)
        total += r1->conn[0] + r1->conn[1] + r1->conn[2] + r1->ingraph;
    return total;
}

int
zero_words(int *p, int n)
{
    while (n-- > 0)
        *p++ = 0;
    return 1;
}

int
main(int argc, char **argv, char **envp)
{
    int e = 0;
    auto int this = 3;
    register int new = 4;
    int class = 5;
    char *list[4];
    char **walker, *one;
    THING a, b, *tp;
    extern int seed;
    extern char names[];
    int count_strings();
    char *esc = "\033[2J\007";

    /* 1 */
    if (sum(3, 10, 20, 12) != 42) e++;
    if (sum(0) != 0) e++;
    if (count_args("%d %s %c", 42, "ok", 'z') != 3) e++;
    if (count_args("plain") != 0) e++;
    if (sum(2, count_args("%d", 42), sum(1, 40)) != 41) e++;
    if (seed == 7 && count_args("%d%d", 42, 42) == 2)
        seed = 8;
    else
        e++;

    /* 2 */
    list[0] = "one"; list[1] = "two"; list[2] = "three"; list[3] = NULL;
    if (count_strings(list) != 3) e++;
    walker = (char **) list;
    one = walker[1];
    if (one[0] != 't' || (*(walker + 2))[4] != 'e') e++;

    /* 3 */
    if (rooms_connected() != 5) e++;
    a.t_pos.x = 3; a.t_pos.y = 4;
    if (a._t._t_pos.x + a.t_pos.y != 7) e++;

    /* 4 */
    fatal("x marks");
    fatal("nothing");
    leave(5);
    if (fatal_calls != 1 || leave_calls != 5) e++;
    if (names[1] != 'b' || seed != 8) e++;

    /* 5 */
    start_daemon(doctor, 0, 0);
    start_daemon((void (*)())turn_see, 1, 2);
    do_daemons();
    if (daemon_total != 101) e++;
    if (find_slot(doctor) != &d_list[0]) e++;
    if (find_slot((void (*)())turn_see) != &d_list[2]) e++;
    if (find_slot(leave) != NULL) e++;
    if (handlers[0].h_func != doctor || handlers[1].h_func != NULL) e++;
    (*handlers[0].h_func)();
    if (daemon_total != 102) e++;

    /* 6 */
    if (pack != NULL || last_message != NULL) e++;
    if (pack) e++;
    if (!pack) seed = 9; else e++;
    zero_words((int *) &a, sizeof a / sizeof (int));
    zero_words((int *) &b, sizeof b / sizeof (int));
    if (a.o_label != NULL || a.l_next != NULL) e++;
    if (a.o_label) e++;
    b.o_count = 2;
    attach(&pack, &a);
    attach(&pack, &b);
    if (pack != &b || next(pack) != &a || next(&a) != NULL) e++;
    if (!(pack && pack->o_count == 2)) e++;
    if (next(&a) && next(&a)->o_count == 99) e++;
    class = 0;
    for (tp = pack; tp; tp = next(tp))
        class++;
    if (class != 2) e++;
    tp = pack;
    while (tp != NULL && tp->o_count != 0)
        tp = next(tp);
    if (tp != &a) e++;
    last_message = (seed == 9) ? "set" : NULL;
    if (last_message == NULL || !last_message) e++;

    /* 7 */
    if (NCOLORS != 5 || NCORNERS != 3) e++;
    if (sizeof used / sizeof (int) != 5) e++;
    if (sizeof msgbuf != 2 * (80 - 9) + 1) e++;
    if (sizeof (coord) != 2 * sizeof (int)) e++;

    /* 8 */
    if (ISHELD != 256 || ISRUN != 4) e++;
    if ((ISHELD | ISRUN) != 260) e++;
    if (CTRL('P') != 16 || 0777 != 511) e++;

    /* 9 */
    if (argc < 1 || argv == NULL || argv[0] == NULL) e++;
    if (argv[argc] != NULL) e++;
    if (envp == NULL) e++;

    /* 10 */
    (void) sum(1, 1);
    (void) e;
    if (this + new != 7) e++;

    /* 11 */
    if (esc[0] != 27 || esc[1] != '[' || esc[4] != 7 || esc[5] != '\0') e++;
    if (max_stats.s_str != 16 || max_stats.s_maxhp != 12) e++;
    if (max_stats.s_dmg[0] != '1' || max_stats.s_dmg[2] != '4' || max_stats.s_dmg[3] != '\0') e++;

    test_errors = e;
#ifndef __V32CXX__
    printf("test_errors = %d\n", test_errors);
#endif
    return 0;
}
