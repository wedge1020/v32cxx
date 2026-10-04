/* ****************************************************************************
 *  libc/v32libc.h -- the slice of the standard C library an ordinary C
 *  program expects, for the Vircon32 console, through v32c++.
 *
 *  Every header in this directory (stdio.h, stdlib.h, ctype.h, ...) just
 *  includes this one, so a program keeps its own #include lines untouched:
 *
 *      v32c++ -I <v32c++>/libc ... program.c
 *
 *  This file is DECLARATIONS only. The code is in v32libc.c, which the
 *  program's one translation unit must #include once (Vircon32 C builds a
 *  single file; there is no linker):
 *
 *      #include <v32libc.c>
 *
 *  The Vircon32 SDK's own headers already provide part of libc under the
 *  standard names. Where an SDK function behaves as C says, it is used as
 *  it is:
 *      string.h  strlen strcmp strncmp
 *                isdigit isalpha isupper islower isspace tolower toupper
 *      misc.h    memset memcpy memcmp rand srand
 *  Where it does not, the standard name is routed (by a macro, below) to a
 *  conforming v32_ function that wraps it:
 *      malloc calloc realloc free   the SDK's speak Vircon32 C's NULL, -1
 *      strcpy strcat strncpy strncat   the SDK's return nothing, and its
 *                                   strncpy writes one word past the limit
 *      exit                         the SDK's takes no argument and halts
 *
 *  Everything is one 32-bit word on Vircon32 -- char, short, int, long and
 *  every pointer -- so sizeof(char) == sizeof(int) == 1 and a string is an
 *  array of words. Code that assumes 8-bit chars only by habit (`char
 *  buf[80]`) is unaffected.
 *
 *  The null pointer is 0, as in C (v32c++ sees to that for C input; see
 *  docs/VIRCON32_QUIRKS.md). Never hand it to an SDK function directly:
 *  the SDK's idea of "no pointer" is -1.
 * ****************************************************************************/
#ifndef V32LIBC_H
#define V32LIBC_H

#include "video.h"
#include "input.h"
#include "time.h"
#include "string.h"
#include "misc.h"
#include <stdarg.h>

/* ---- stddef / limits ---------------------------------------------------- */
typedef int size_t;
typedef int time_t;
typedef int off_t;
typedef int pid_t;
typedef int uid_t;
typedef int gid_t;
typedef int mode_t;

#define INT_MAX     2147483647
#define INT_MIN     (-2147483647 - 1)
#define UINT_MAX    INT_MAX
#define LONG_MAX    INT_MAX
#define LONG_MIN    INT_MIN
#define SHRT_MAX    32767
#define CHAR_BIT    32
#define PATH_MAX    256

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* ---- errno -------------------------------------------------------------- */
extern int errno;
#define ENOENT  2       /* no such file */
#define ENOMEM  12
#define EACCES  13      /* the memory card holds another game's data */
#define ENODEV  19      /* no memory card */
#define EMFILE  24
#define ENOSPC  28      /* the memory card is full */
char *strerror(int error);

/* ---- ctype (the rest is in the SDK's string.h) -------------------------- */
int isprint(int c);
int isalnum(int c);
int ispunct(int c);
int iscntrl(int c);
int toascii(int c);

/* ---- string (the rest is in the SDK's string.h) ------------------------- */
char *strchr(char *s, int c);
char *strrchr(char *s, int c);
char *strstr(char *haystack, char *needle);

/* The four below return their destination, as C's do (the SDK's return
 * nothing, so `strcat(strcpy(buf, a), b)` would not compile), and strncpy
 * is C's: at most n words written, zero-filled, and no terminator when the
 * source is n characters or longer. */
char *v32_strcpy(char *dest, char *src);
char *v32_strcat(char *dest, char *src);
char *v32_strncpy(char *dest, char *src, int n);
char *v32_strncat(char *dest, char *src, int n);
#define strcpy(dest, src)       v32_strcpy(dest, src)
#define strcat(dest, src)       v32_strcat(dest, src)
#define strncpy(dest, src, n)   v32_strncpy(dest, src, n)
#define strncat(dest, src, n)   v32_strncat(dest, src, n)

/* ---- stdlib ------------------------------------------------------------- */
/* The SDK's allocator reports failure with Vircon32 C's own NULL, which is
 * -1, and would take a C null pointer (0) for a block at address 0. These
 * wrappers speak C: NULL is returned on failure, free(NULL) does nothing,
 * realloc(NULL, n) allocates. */
void *v32_malloc(int size);
void *v32_calloc(int number, int size);
void *v32_realloc(void *ptr, int size);
void v32_free(void *ptr);
#define malloc(size)            v32_malloc(size)
#define calloc(number, size)    v32_calloc(number, size)
#define realloc(ptr, size)      v32_realloc(ptr, size)
#define free(ptr)               v32_free(ptr)

int atoi(char *s);
#ifndef MATH_H
int abs(int n);
#endif
char *getenv(char *name);
void abort(void);

/* exit(): the SDK's own takes no argument and halts the console.
 * v32_exit() flushes the terminal, waits for START, then restarts the
 * cartridge -- what leaving a program means on a console. A program that
 * wants something else sets v32_exit_hook. */
void v32_exit(int status);
extern void (*v32_exit_hook)(int status);
extern char *v32_exit_prompt;           /* what is shown while waiting for START */
#define exit(status) v32_exit(status)

/* ---- stdio -------------------------------------------------------------- */
/* Output to stdout/stderr goes to the screen terminal (see below), and
 * stdin reads the gamepad's keyboard. Every other FILE is a file on the
 * memory card: see v32file.c, which also says what "a file" is here
 * (whole files held in RAM while open, sizes in words). */
#define V32FILE_MAX_FILES   8
#define V32FILE_NAME_LEN    24
typedef struct v32_file {
    int fd;                 /* 0, 1, 2: the standard streams; 3: a card file */
    int open;
    int readable;
    int writable;
    int dirty;              /* changed since it was last written to the card */
    int error;
    int eof;
    int *data;              /* the whole file */
    int size;
    int capacity;
    int pos;
    char name[V32FILE_NAME_LEN];
} FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF     (-1)
#define BUFSIZ  512
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int printf(char *fmt, ...);
int fprintf(FILE *f, char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);
int vsprintf(char *buf, char *fmt, va_list args);
int putchar(int c);
int puts(char *s);
int fputs(char *s, FILE *f);
int fputc(int c, FILE *f);
int getchar(void);
char *fgets(char *buf, int size, FILE *f);
int fflush(FILE *f);
void setbuf(FILE *f, char *buf);
int sscanf(char *s, char *fmt, ...);     /* %d %u %x %c %s, with h / l and a width */
FILE *fopen(char *name, char *mode);    /* "r" "w" "a" "r+" "w+" */
int fclose(FILE *f);
size_t fread(void *ptr, size_t size, size_t count, FILE *f);
size_t fwrite(void *ptr, size_t size, size_t count, FILE *f);
int fgetc(FILE *f);
#define getc(f)     fgetc(f)
#define putc(c, f)  fputc(c, f)
int fseek(FILE *f, int offset, int whence);
int ftell(FILE *f);
void rewind(FILE *f);
int feof(FILE *f);
int ferror(FILE *f);
int remove(char *name);
void perror(char *s);

/* ---- unistd / sys/stat --------------------------------------------------- */
struct stat {
    int st_mode;
    int st_size;            /* in words */
    int st_nlink;
    int st_uid;
    int st_mtime;
};
int stat(char *name, struct stat *st);  /* 0, or -1 when there is no such file */
int unlink(char *name);
extern char **environ;                  /* always empty */

/* ---- the memory card ------------------------------------------------------
 * Files live on the memory card, under the program's own signature (the
 * first 20 words of a card say which game its data belongs to). Set
 * v32file_signature before the first file is opened. */
extern char *v32file_signature;
#define V32FILE_NO_CARD 0
#define V32FILE_READY   1               /* this program's card */
#define V32FILE_BLANK   2               /* an empty card: formatted on first write */
#define V32FILE_FOREIGN 3               /* another game's data: never written */
int  v32file_card(void);
void v32file_format(void);              /* erases the card and makes it this program's */
int  v32file_free_words(void);

/* ---- signal ------------------------------------------------------------- */
/* No signals on a console: signal() remembers nothing and returns SIG_DFL. */
typedef void (*v32_sighandler)(int sig);
#define SIG_DFL ((v32_sighandler)0)
#define SIG_IGN ((v32_sighandler)0)
#define SIG_ERR ((v32_sighandler)0)
#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGTRAP  5
#define SIGABRT  6
#define SIGFPE   8
#define SIGKILL  9
#define SIGBUS  10
#define SIGSEGV 11
#define SIGSYS  12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
v32_sighandler signal(int sig, v32_sighandler handler);

/* ---- time ---------------------------------------------------------------
 * The SDK's time.h has the console's own clock (get_date, get_time,
 * get_frame_counter). These are the C names on top of it. */
struct tm {
    int tm_sec, tm_min, tm_hour;
    int tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};
time_t time(time_t *t);
struct tm *localtime(time_t *t);

/* ---- the screen terminal ------------------------------------------------
 * What printf/putchar write to and getchar/fgets read from: an 80x24
 * teletype drawn with the BIOS font (see v32term.c). curses.h draws on the
 * same screen. */
void v32term_putc(int c);
void v32term_puts(char *s);
int  v32term_getkey(void);

#endif /* V32LIBC_H */
