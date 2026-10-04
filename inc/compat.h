#ifndef COMPAT_H
#define COMPAT_H

/*
 * compat.h -- the handful of things v32c++ needs from the operating
 * system that are not in standard C, behind one interface, so the rest
 * of the code builds unchanged with GCC on Linux and macOS and with
 * MinGW GCC on Windows (MSYS2).
 *
 * MinGW's C library has no realpath() and no getline(), which is what
 * stopped the Windows build ("implicit declaration of function"), and no
 * dependable strndup(). Rather than #ifdef each call site, the three
 * that are easy to write in standard C (getline, strndup, last path
 * separator) are simply v32c++'s own on EVERY platform -- so the same
 * code that runs on Windows is exercised by every Linux test run. Only
 * what genuinely differs per system is switched on _WIN32 in compat.c:
 * canonical paths, temporary files, and the executable-file test.
 */

#include <stdio.h>
#include <stddef.h>

/* Separates the entries of a directory list in an environment variable:
 * ':' on POSIX, ';' on Windows (where ':' follows the drive letter).
 * $PATH, $V32CXX_INCLUDE and $V32CXX_SDK_INCLUDE are all split on it. */
#ifdef _WIN32
#define COMPAT_PATH_LIST_SEP ';'
#else
#define COMPAT_PATH_LIST_SEP ':'
#endif

/* The canonical absolute form of `path` (realpath / _fullpath), in
 * malloc'd storage, or NULL if it cannot be resolved. */
char *compat_realpath(const char *path);

/* POSIX getline(): reads one line, newline included, into *line (grown
 * with realloc as needed; *line may start NULL with *cap 0). Returns the
 * number of characters read, or -1 at end of file with nothing read. */
long compat_getline(char **line, size_t *cap, FILE *in);

/* POSIX strndup(): a malloc'd copy of at most n characters of s. */
char *compat_strndup(const char *s, size_t n);

/* A temporary file open for update that disappears when closed, like
 * tmpfile(). (The C library's own tmpfile() on Windows creates its file
 * in the root of the current drive, which an ordinary user usually may
 * not write to.) NULL on failure, with errno set. */
FILE *compat_tmpfile(void);

/* Is `path` a program that can be run? On Windows `path` may be given
 * without its ".exe". */
int compat_is_executable(const char *path);

/* The last directory separator in `path` ('/', and on Windows also
 * '\\'), or NULL if it has none. */
const char *compat_last_separator(const char *path);

#endif /* COMPAT_H */
