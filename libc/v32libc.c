/* ****************************************************************************
 *  libc/v32libc.c -- see v32libc.h. #include this once in the program's
 *  one translation unit.
 * ****************************************************************************/
#ifndef V32LIBC_C
#define V32LIBC_C

#include "v32libc.h"
#include "v32term.c"

int errno = 0;

int v32file_close(FILE *f);
int v32file_flush(FILE *f);
int v32file_getc(FILE *f);
int v32file_putc(int c, FILE *f);
#include "v32file.c"

/* ---- ctype ---------------------------------------------------------------- */

int isprint(int c)
{
    return c >= ' ' && c < 127;
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int ispunct(int c)
{
    return isprint(c) && c != ' ' && !isalnum(c);
}

int iscntrl(int c)
{
    return (c >= 0 && c < ' ') || c == 127;
}

int toascii(int c)
{
    return c & 127;
}

/* ---- string --------------------------------------------------------------- */

char *strchr(char *s, int c)
{
    for (;;) {
        if (*s == c)
            return s;
        if (*s == '\0')
            return NULL;
        s++;
    }
}

char *strrchr(char *s, int c)
{
    char *last = NULL;

    for (;;) {
        if (*s == c)
            last = s;
        if (*s == '\0')
            return last;
        s++;
    }
}

char *strstr(char *haystack, char *needle)
{
    int n = strlen(needle);

    if (n == 0)
        return haystack;
    while (*haystack != '\0') {
        if (*haystack == *needle && strncmp(haystack, needle, n) == 0)
            return haystack;
        haystack++;
    }
    return NULL;
}

char *v32_strcpy(char *dest, char *src)
{
    char *d = dest;

    while ((*d++ = *src++) != '\0')
        continue;
    return dest;
}

char *v32_strcat(char *dest, char *src)
{
    char *d = dest;

    while (*d != '\0')
        d++;
    while ((*d++ = *src++) != '\0')
        continue;
    return dest;
}

char *v32_strncpy(char *dest, char *src, int n)
{
    int i = 0;

    while (i < n && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    while (i < n)
        dest[i++] = '\0';
    return dest;
}

char *v32_strncat(char *dest, char *src, int n)
{
    char *d = dest;

    while (*d != '\0')
        d++;
    while (n-- > 0 && *src != '\0')
        *d++ = *src++;
    *d = '\0';
    return dest;
}

/* ---- stdlib --------------------------------------------------------------- */

/* The four allocator wrappers call the SDK's own functions, so the macros
 * that route everyone else here are off for their duration. */
#undef malloc
#undef calloc
#undef realloc
#undef free

#define V32_SDK_NULL (-1)       /* what Vircon32 C's NULL is */

void *v32_malloc(int size)
{
    void *p = malloc(size);

    if ((int)p == V32_SDK_NULL)
        return NULL;
    return p;
}

void *v32_calloc(int number, int size)
{
    void *p = malloc(number * size);

    if ((int)p == V32_SDK_NULL)
        return NULL;
    memset(p, 0, number * size);
    return p;
}

void v32_free(void *ptr)
{
    if (ptr != NULL)
        free(ptr);
}

void *v32_realloc(void *ptr, int size)
{
    void *p;

    if (ptr == NULL)
        return v32_malloc(size);
    if (size <= 0) {
        free(ptr);
        return NULL;
    }
    p = realloc(ptr, size);
    if ((int)p == V32_SDK_NULL)
        return NULL;
    return p;
}

#define malloc(size)            v32_malloc(size)
#define calloc(number, size)    v32_calloc(number, size)
#define realloc(ptr, size)      v32_realloc(ptr, size)
#define free(ptr)               v32_free(ptr)

int atoi(char *s)
{
    int n = 0;
    int negative = 0;

    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-') {
        negative = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return negative ? -n : n;
}

#ifndef MATH_H
int abs(int n)
{
    return n < 0 ? -n : n;
}
#endif

char *getenv(char *name)
{
    name = name;
    return NULL;
}

/* Starts the cartridge again from nothing: RAM cleared, stack reset, the
 * program's globals re-initialized, main() entered afresh. */
static void v32_restart(void)
{
    asm {
        "mov SP, 0x003FFFFF"
        "mov BP, SP"
        "mov DR, 0"
        "mov SR, 0"
        "mov CR, 0x003FFF00"
        "sets"
        "call __global_scope_initialization"
        "call __function_main"
        "hlt"
    }
}

void (*v32_exit_hook)(int status) = NULL;
char *v32_exit_prompt = "\n[Press START to play again]";

void v32_exit(int status)
{
    if (v32_exit_hook != NULL)
        v32_exit_hook(status);
    v32term_attr(V32TERM_INK(V32INK_YELLOW));
    v32term_puts(v32_exit_prompt);
    v32term_attr(0);
    do {
        v32pad_poll();
    } while (!v32pad_hit(V32PAD_START));
    clear_screen(color_black);
    end_frame();
    v32_restart();
}

void abort(void)
{
    v32_exit(1);
}

/* ---- stdio ---------------------------------------------------------------- */

static FILE v32_stdin_file = { 0 };
static FILE v32_stdout_file = { 1 };
static FILE v32_stderr_file = { 2 };
FILE *stdin = &v32_stdin_file;
FILE *stdout = &v32_stdout_file;
FILE *stderr = &v32_stderr_file;

/* One conversion's worth of text into `out`, padded to `width`.
 * Returns the new end of the output. */
static char *v32_fmt_pad(char *out, char *text, int len, int width, int left, int zero)
{
    int pad = width - len;
    int i;

    if (!left)
        for (i = 0; i < pad; i++)
            *out++ = zero ? '0' : ' ';
    for (i = 0; i < len; i++)
        *out++ = text[i];
    if (left)
        for (i = 0; i < pad; i++)
            *out++ = ' ';
    return out;
}

/* Digits of `value` in `base`, most significant first. Returns the count. */
static int v32_fmt_digits(char *digits, int value, int base, int upper)
{
    char tmp[36];
    int n = 0;
    int i, d;

    if (value == 0)
        tmp[n++] = '0';
    while (value != 0) {
        d = value % base;
        if (d < 0)
            d = -d;
        if (d < 10)
            tmp[n++] = (char)('0' + d);
        else
            tmp[n++] = (char)((upper ? 'A' : 'a') + d - 10);
        value /= base;
    }
    for (i = 0; i < n; i++)
        digits[i] = tmp[n - 1 - i];
    return n;
}

/* Supports %d %i %u %x %X %o %c %s %% with the - and 0 flags, a width and
 * a precision (either may be *), and the h/l length modifiers (every
 * integer is one word here, so they change nothing). */
int vsprintf(char *buf, char *fmt, va_list args)
{
    char *out = buf;
    char number[40];
    char *text;
    int left, zero, width, precision, len, value, negative, c;

    while (*fmt != '\0') {
        if (*fmt != '%') {
            *out++ = *fmt++;
            continue;
        }
        fmt++;
        left = 0;
        zero = 0;
        width = 0;
        precision = -1;
        while (*fmt == '-' || *fmt == '0' || *fmt == '+' || *fmt == ' ' || *fmt == '#') {
            if (*fmt == '-')
                left = 1;
            if (*fmt == '0')
                zero = 1;
            fmt++;
        }
        if (*fmt == '*') {
            width = va_arg(args, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + (*fmt++ - '0');
        }
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt == '*') {
                precision = va_arg(args, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9')
                    precision = precision * 10 + (*fmt++ - '0');
            }
        }
        while (*fmt == 'l' || *fmt == 'h')
            fmt++;
        c = *fmt;
        if (c == '\0')
            break;
        fmt++;
        if (c == 'd' || c == 'i' || c == 'u') {
            value = va_arg(args, int);
            negative = (value < 0);
            len = 0;
            if (negative)
                number[len++] = '-';
            len += v32_fmt_digits(&number[len], value, 10, 0);
            if (negative && zero && !left && width > len) {
                /* the sign goes ahead of the zero padding */
                *out++ = '-';
                out = v32_fmt_pad(out, &number[1], len - 1, width - 1, 0, 1);
            } else {
                out = v32_fmt_pad(out, number, len, width, left, zero);
            }
        } else if (c == 'x' || c == 'X' || c == 'o') {
            value = va_arg(args, int);
            if (value < 0) {
                /* no unsigned type: print the low 31 bits */
                value &= INT_MAX;
            }
            len = v32_fmt_digits(number, value, c == 'o' ? 8 : 16, c == 'X');
            out = v32_fmt_pad(out, number, len, width, left, zero);
        } else if (c == 'c') {
            number[0] = (char)va_arg(args, int);
            out = v32_fmt_pad(out, number, 1, width, left, 0);
        } else if (c == 's') {
            text = va_arg(args, char *);
            if (text == NULL)
                text = "(null)";
            len = strlen(text);
            if (precision >= 0 && len > precision)
                len = precision;
            out = v32_fmt_pad(out, text, len, width, left, 0);
        } else {
            *out++ = (char)c;
        }
    }
    *out = '\0';
    return (int)(out - buf);
}

int sprintf(char *buf, char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsprintf(buf, fmt, args);
    va_end(args);
    return n;
}

static char v32_printf_buf[2048];

int printf(char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsprintf(v32_printf_buf, fmt, args);
    va_end(args);
    v32term_puts(v32_printf_buf);
    return n;
}

int fprintf(FILE *f, char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsprintf(v32_printf_buf, fmt, args);
    va_end(args);
    if (f == stdout || f == stderr)
        v32term_puts(v32_printf_buf);
    return n;
}

int putchar(int c)
{
    v32term_putc(c);
    return c;
}

int fputc(int c, FILE *f)
{
    if (f == stdout || f == stderr) {
        v32term_putc(c);
        return c;
    }
    return v32file_putc(c, f);
}

int fgetc(FILE *f)
{
    if (f == stdin)
        return getchar();
    return v32file_getc(f);
}

int fputs(char *s, FILE *f)
{
    if (f == stdout || f == stderr)
        v32term_puts(s);
    return 0;
}

int puts(char *s)
{
    v32term_puts(s);
    v32term_putc('\n');
    return 0;
}

int fflush(FILE *f)
{
    v32term_flush();
    return v32file_flush(f);
}

void setbuf(FILE *f, char *buf)
{
    f = f;
    buf = buf;
}

int getchar(void)
{
    int c = v32term_getkey();

    if (c == '\r')
        c = '\n';
    v32term_putc(c);
    return c;
}

char *fgets(char *buf, int size, FILE *f)
{
    int n = 0;
    int c;

    if (f != stdin)
        return NULL;
    while (n < size - 1) {
        c = v32term_getkey();
        if (c == '\r')
            c = '\n';
        if (c == V32KEY_BACKSPACE) {
            if (n > 0) {
                n--;
                v32term_puts("\b \b");
            }
            continue;
        }
        if (c == V32KEY_ESC)
            c = '\n';
        v32term_putc(c);
        buf[n++] = (char)c;
        if (c == '\n')
            break;
    }
    buf[n] = '\0';
    return buf;
}

int fclose(FILE *f)
{
    return v32file_close(f);
}

char *strerror(int error)
{
    switch (error) {
        case 0:      return "No error";
        case ENOENT: return "No such file";
        case ENOMEM: return "Out of memory";
        case EACCES: return "The memory card holds another game's data";
        case ENODEV: return "No memory card";
        case EMFILE: return "Too many open files";
        case ENOSPC: return "The memory card is full";
    }
    return "Error";
}

void perror(char *s)
{
    if (s != NULL && s[0] != '\0') {
        v32term_puts(s);
        v32term_puts(": ");
    }
    v32term_puts(strerror(errno));
    v32term_putc('\n');
}

char **environ = NULL;

/* ---- sscanf ----------------------------------------------------------------- */

static int v32_scan_digit(int c, int base)
{
    int d = -1;

    if (c >= '0' && c <= '9')
        d = c - '0';
    else if (c >= 'a' && c <= 'f')
        d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        d = c - 'A' + 10;
    return (d >= 0 && d < base) ? d : -1;
}

int sscanf(char *s, char *fmt, ...)
{
    va_list args;
    int count = 0;
    int width, base, negative, value, digits, d;
    char *out;
    int *number;

    va_start(args, fmt);
    while (*fmt != '\0') {
        if (isspace(*fmt)) {
            while (isspace(*s))
                s++;
            fmt++;
            continue;
        }
        if (*fmt != '%') {
            if (*s != *fmt)
                break;
            s++;
            fmt++;
            continue;
        }
        fmt++;
        width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'h' || *fmt == 'l')
            fmt++;
        if (*fmt == 'c') {
            if (*s == '\0')
                break;
            out = va_arg(args, char *);
            *out = *s++;
            count++;
        } else if (*fmt == 's') {
            while (isspace(*s))
                s++;
            if (*s == '\0')
                break;
            out = va_arg(args, char *);
            digits = 0;
            while (*s != '\0' && !isspace(*s) && (width == 0 || digits < width)) {
                *out++ = *s++;
                digits++;
            }
            *out = '\0';
            count++;
        } else if (*fmt == 'd' || *fmt == 'u' || *fmt == 'x' || *fmt == 'i') {
            base = (*fmt == 'x') ? 16 : 10;
            while (isspace(*s))
                s++;
            negative = 0;
            if (*s == '-' || *s == '+') {
                negative = (*s == '-');
                s++;
            }
            value = 0;
            digits = 0;
            while ((d = v32_scan_digit(*s, base)) >= 0 && (width == 0 || digits < width)) {
                value = value * base + d;
                digits++;
                s++;
            }
            if (digits == 0)
                break;
            number = va_arg(args, int *);
            *number = negative ? -value : value;
            count++;
        } else {
            break;
        }
        fmt++;
    }
    va_end(args);
    return count;
}

/* ---- signal --------------------------------------------------------------- */

v32_sighandler signal(int sig, v32_sighandler handler)
{
    sig = sig;
    handler = handler;
    return SIG_DFL;
}

/* ---- time ----------------------------------------------------------------- */

/* Not seconds since 1970: the console's date and time of day packed into
 * one word, plus the frame counter so two calls in one second differ. It
 * is good for what programs here use it for -- a random seed, and
 * localtime(). */
time_t time(time_t *t)
{
#ifdef V32LIBC_FIXED_TIME
    /* for reproducible test runs: v32c++ -DV32LIBC_FIXED_TIME=12345 */
    time_t now = V32LIBC_FIXED_TIME;
#else
    time_t now = get_date() * 86400 + get_time();
#endif

    if (t != NULL)
        *t = now;
    return now;
}

static struct tm v32_tm;

struct tm *localtime(time_t *t)
{
    static int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int date = get_date();      /* year << 16 | day of the year */
    int clock = get_time();     /* seconds since midnight */
    int year = date >> 16;
    int day = date & 0xFFFF;
    int month = 0;
    int leap = (year % 4 == 0) && (year % 100 != 0);
    int n;

    t = t;
    v32_tm.tm_yday = day;
    while (month < 11) {
        n = month_days[month];
        if (month == 1 && leap)
            n = 29;
        if (day < n)
            break;
        day -= n;
        month++;
    }
    v32_tm.tm_sec = clock % 60;
    v32_tm.tm_min = (clock % 3600) / 60;
    v32_tm.tm_hour = clock / 3600;
    v32_tm.tm_mday = day + 1;
    v32_tm.tm_mon = month;
    v32_tm.tm_year = year - 1900;
    v32_tm.tm_wday = 0;
    v32_tm.tm_isdst = 0;
    return &v32_tm;
}

#endif /* V32LIBC_C */
