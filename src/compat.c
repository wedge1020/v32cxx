/*
 * compat.c -- see compat.h.
 */
#include <stdlib.h>
#include <string.h>
#include "compat.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

char *compat_realpath(const char *path) {
#ifdef _WIN32
    /* _fullpath resolves "." and ".." and makes the path absolute. Unlike
     * realpath it does not check that the file exists and does not follow
     * links; for telling two spellings of one header apart, which is all
     * this is used for, that is enough. */
    return _fullpath(NULL, path, 0);
#else
    return realpath(path, NULL);
#endif
}

long compat_getline(char **line, size_t *cap, FILE *in) {
    if (*line == NULL || *cap == 0) {
        *cap = 128;
        *line = realloc(*line, *cap);
        if (*line == NULL) return -1;
    }
    size_t len = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (len + 2 > *cap) {               /* room for this char and the '\0' */
            size_t bigger = *cap * 2;
            char *grown = realloc(*line, bigger);
            if (grown == NULL) return -1;
            *line = grown;
            *cap = bigger;
        }
        (*line)[len++] = (char)c;
        if (c == '\n') break;
    }
    if (len == 0) return -1;                /* end of file, nothing read */
    (*line)[len] = '\0';
    return (long)len;
}

char *compat_strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len] != '\0') len++;
    char *copy = malloc(len + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

FILE *compat_tmpfile(void) {
#ifdef _WIN32
    /* A uniquely named file in the user's own temporary directory, opened
     * with the C runtime's "D" flag: deleted when the last handle to it
     * is closed. */
    char dir[MAX_PATH + 1];
    char name[MAX_PATH + 1];
    DWORD n = GetTempPathA(sizeof dir, dir);
    if (n == 0 || n >= sizeof dir) return NULL;
    if (GetTempFileNameA(dir, "v32", 0, name) == 0) return NULL;
    FILE *f = fopen(name, "w+bD");
    if (f == NULL) DeleteFileA(name);
    return f;
#else
    return tmpfile();
#endif
}

int compat_is_executable(const char *path) {
#ifdef _WIN32
    /* Windows has no execute permission bit to ask about: the file
     * existing is the test, with or without the ".exe". */
    if (_access(path, 0) == 0) return 1;
    size_t len = strlen(path);
    char *exe = malloc(len + 5);
    if (exe == NULL) return 0;
    memcpy(exe, path, len);
    memcpy(exe + len, ".exe", 5);
    int ok = _access(exe, 0) == 0;
    free(exe);
    return ok;
#else
    return access(path, X_OK) == 0;
#endif
}

const char *compat_last_separator(const char *path) {
    const char *last = strrchr(path, '/');
#ifdef _WIN32
    const char *back = strrchr(path, '\\');
    if (back != NULL && (last == NULL || back > last)) last = back;
#endif
    return last;
}
