#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "pathutil.h"
#include "compat.h"

char *replace_extension(const char *path, const char *new_ext) {
    const char *dot = strrchr(path, '.');
    const char *slash = compat_last_separator(path);
    size_t base_len = (dot != NULL && (slash == NULL || dot > slash))
                     ? (size_t)(dot - path)
                     : strlen(path);
    size_t ext_len = strlen(new_ext);
    char *result = malloc(base_len + ext_len + 1);
    memcpy(result, path, base_len);
    memcpy(result + base_len, new_ext, ext_len);
    result[base_len + ext_len] = '\0';
    return result;
}

const char *path_basename(const char *path) {
    const char *slash = compat_last_separator(path);
    return (slash != NULL) ? slash + 1 : path;
}

char *path_dirname(const char *path) {
    const char *slash = compat_last_separator(path);
    if (slash == NULL) return strdup(".");
    if (slash == path) return strdup("/");   /* "/foo" -> "/" */
    size_t len = (size_t)(slash - path);
    char *result = malloc(len + 1);
    memcpy(result, path, len);
    result[len] = '\0';
    return result;
}

/* "C:" -- a Windows drive component (a single letter and a colon). */
static int is_drive_part(const char *s) {
    return s[0] != '\0' && s[1] == ':' && s[2] == '\0' && isalpha((unsigned char)s[0]);
}

/* Component equality: Windows paths are case-insensitive. */
static int part_equal(const char *a, const char *b) {
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}

/* Splits `path` (made absolute against the cwd when it is relative)
 * into normalized components, in place into `parts`; returns the count.
 * "." is dropped and ".." pops the previous component. */
static int split_normalized(const char *path, char **parts, int max_parts) {
    char cwd[4096];
    size_t cap = strlen(path) + sizeof cwd + 2;
    char *full = malloc(cap);
    int has_drive = (path[0] != '\0' && isalpha((unsigned char)path[0]) && path[1] == ':');
    int is_abs = has_drive;
    full[0] = '\0';
    if (!has_drive && (path[0] == '/' || path[0] == '\\')) {
        /* "/dir" or "\\dir": rooted, but on Windows still on the current
         * drive. Take that drive from the cwd so it is not lost (and so
         * a leading "/C:/dir" -- a drive after the root -- passes as is). */
        is_abs = 1;
#ifdef _WIN32
        if (path[1] != '/' && path[1] != '\\' && !(isalpha((unsigned char)path[1]) && path[2] == ':') &&
            getcwd(cwd, sizeof cwd) != NULL && isalpha((unsigned char)cwd[0]) && cwd[1] == ':') {
            full[0] = cwd[0]; full[1] = ':'; full[2] = '\0';
        }
#endif
    }
    if (!is_abs && getcwd(cwd, sizeof cwd) != NULL) {
        strcat(full, cwd);
        strcat(full, "/");
    }
    strcat(full, path);

    int count = 0;
    for (char *tok = full; *tok != '\0'; ) {
        char *end = tok;
        while (*end != '\0' && *end != '/' && *end != '\\') end++;
        char saved = *end;
        *end = '\0';
        if (tok[0] == '\0' || strcmp(tok, ".") == 0) {
            /* skip */
        } else if (strcmp(tok, "..") == 0) {
            if (count > 0) { free(parts[--count]); }
        } else if (count < max_parts) {
            parts[count++] = strdup(tok);
        }
        if (saved == '\0') break;
        tok = end + 1;
    }
    free(full);
    return count;
}

char *path_relative_to(const char *from_dir, const char *target) {
    enum { MAXP = 256 };
    char *from_parts[MAXP], *to_parts[MAXP];
    int nf = split_normalized(from_dir, from_parts, MAXP);
    int nt = split_normalized(target, to_parts, MAXP);

    /* Different drives have no relative path between them: the answer is
     * the target itself, drive and all ("../../D:/x" would be nonsense). */
    if (nf > 0 && nt > 0 && is_drive_part(from_parts[0]) && is_drive_part(to_parts[0]) &&
        !part_equal(from_parts[0], to_parts[0])) {
        size_t abs_cap = 4;
        for (int i = 0; i < nt; i++) abs_cap += strlen(to_parts[i]) + 1;
        char *abs_result = malloc(abs_cap);
        abs_result[0] = '\0';
        for (int i = 0; i < nt; i++) {
            strcat(abs_result, to_parts[i]);
            if (i + 1 < nt) strcat(abs_result, "/");
        }
        for (int i = 0; i < nf; i++) free(from_parts[i]);
        for (int i = 0; i < nt; i++) free(to_parts[i]);
        return abs_result;
    }

    int common = 0;
    while (common < nf && common < nt - 1 &&
           part_equal(from_parts[common], to_parts[common])) {
        common++;
    }

    size_t cap = 4;
    for (int i = common; i < nt; i++) cap += strlen(to_parts[i]) + 1;
    cap += (size_t)(nf - common) * 3;
    char *result = malloc(cap);
    result[0] = '\0';
    for (int i = common; i < nf; i++) strcat(result, "../");
    for (int i = common; i < nt; i++) {
        strcat(result, to_parts[i]);
        if (i + 1 < nt) strcat(result, "/");
    }
    for (int i = 0; i < nf; i++) free(from_parts[i]);
    for (int i = 0; i < nt; i++) free(to_parts[i]);
    return result;
}
