#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> /* access() */
#include "prescan.h"

/* ---- tiny growable path-set --------------------------------------------
 * Used three times: the #pragma-once set, the inclusion stack, and the
 * filename intern table. All are single-shot, whole-run state that
 * only grows, so one shared implementation serves all three.
 */
typedef struct PathSet {
    char **items;
    int count;
    int capacity;
} PathSet;

static int path_set_contains(const PathSet *set, const char *path) {
    for (int i = 0; i < set->count; i++)
        if (strcmp(set->items[i], path) == 0) return 1;
    return 0;
}

static void path_set_add(PathSet *set, const char *path) {
    if (set->count == set->capacity) {
        set->capacity = set->capacity ? set->capacity * 2 : 8;
        set->items = realloc(set->items, sizeof(char *) * (size_t)set->capacity);
    }
    set->items[set->count++] = strdup(path);
}

static void path_set_pop(PathSet *set) {
    /* Stack use only -- removes the most recently added entry. */
    if (set->count > 0) free(set->items[--set->count]);
}

/* ---- file-path helpers ------------------------------------------------ */

/* Returns a malloc'd copy of `path`'s directory component (no trailing
 * slash), or "." when path has none -- so join_path(dir, "a.hpp") always
 * produces a real sibling lookup. */
static char *path_dirname(const char *path) {
    const char *slash = strrchr(path, '/');
    if (slash == NULL) return strdup(".");
    size_t len = (size_t)(slash - path);
    if (len == 0) len = 1; /* "/file" -> "/" */
    char *dir = malloc(len + 1);
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

/* malloc'd "dir/target". */
static char *join_path(const char *dir, const char *target) {
    char *joined = malloc(strlen(dir) + strlen(target) + 2);
    sprintf(joined, "%s/%s", dir, target);
    return joined;
}

static int ends_with(const char *s, const char *suffix) {
    size_t len = strlen(s), slen = strlen(suffix);
    return len >= slen && strcmp(s + len - slen, suffix) == 0;
}

/* C++ headers/sources this pre-scan takes over from the pass-through.
 * Case-sensitive on purpose: ".HPP"/".Cpp" are not C++ conventions,
 * and being conservative here means an odd extension (".hxx", ".hh")
 * simply keeps today's pass-through behavior instead of surprising
 * anyone. Add to this list if that ever changes. */
static int is_expanded_extension(const char *target) {
    return ends_with(target, ".hpp") || ends_with(target, ".cpp");
}

/* Canonical identity for #pragma-once and cycle detection. realpath()
 * resolves "../", symlinks, and repeated-inclusion-via-different-
 * relative-paths, which is exactly the identity #pragma once needs.
 * Falls back to the path as-given when realpath fails (e.g. a path
 * through a nonexistent-but-irrelevant component), which can only
 * make detection MORE conservative, never wrong. */
static char *canonical_path(const char *path) {
    char *resolved = realpath(path, NULL);
    return resolved ? resolved : strdup(path);
}

/* ---- directive scanning -------------------------------------------------
 *
 * Walks one source line with a small state machine (block comment,
 * line comment, string literal, char literal -- backslash escapes
 * honored in both literal kinds) so a "#" inside any of those is
 * never mistaken for a directive. Reports the "#" if it is the
 * first non-whitespace CODE on the line (comments before it don't
 * count -- same rule real cpp applies, since comment removal comes
 * before directive processing). Also carries *in_block_comment
 * across lines, so directives inside multi-line comments are
 * correctly ignored too.
 */
static const char *find_directive(const char *line, int *in_block_comment) {
    const char *p = line;
    const char *hash = NULL;
    int in_comment = *in_block_comment;
    int in_str = 0, in_chr = 0;
    int saw_code = 0;

    while (*p) {
        if (in_comment) {
            if (p[0] == '*' && p[1] == '/') { in_comment = 0; p++; }
            else p++;
            continue;
        }
        if (in_str) {
            if (p[0] == '\\' && p[1]) p++;
            else if (*p == '"') in_str = 0;
            p++;
            continue;
        }
        if (in_chr) {
            if (p[0] == '\\' && p[1]) p++;
            else if (*p == '\'') in_chr = 0;
            p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '/') break;      /* rest is comment */
        if (p[0] == '/' && p[1] == '*') { in_comment = 1; p += 2; continue; }
        if (*p == '"') { in_str = 1; saw_code = 1; p++; continue; }
        if (*p == '\'') { in_chr = 1; saw_code = 1; p++; continue; }
        if (*p == '#') {
            if (!saw_code) hash = p;
            /* keep scanning: in_comment state must be carried
             * past this point for whatever follows on the line */
            saw_code = 1;
            p++;
            continue;
        }
        if (*p != ' ' && *p != '\t') saw_code = 1;
        p++;
    }
    *in_block_comment = in_comment;
    return hash;
}

/* Parses the directive word after '#' ("include", "pragma", ...).
 * Returns a pointer into `hash`'s line and sets *rest past the word. */
static char *directive_word(const char *hash, const char **rest) {
    const char *p = hash + 1;
    while (*p == ' ' || *p == '\t') p++;
    const char *start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    char *word = malloc((size_t)(p - start) + 1);
    memcpy(word, start, (size_t)(p - start));
    word[p - start] = '\0';
    *rest = p;
    return word;
}

/* From the text after the "include" word, extracts the target of
 * "..." (sets *angle=0) or <...> (sets *angle=1). Returns a malloc'd
 * target, or NULL when the line doesn't look like any #include this
 * pre-scan recognizes -- in which case the caller passes the whole
 * line through verbatim, exactly like any other unrecognized #-line
 * (a deliberately conservative "do nothing surprising" stance). */
static char *include_target(const char *rest, int *angle) {
    const char *p = rest;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
        *angle = 0;
        const char *start = ++p;
        while (*p && *p != '"') p++;
        if (*p != '"') return NULL;
        char *target = malloc((size_t)(p - start) + 1);
        memcpy(target, start, (size_t)(p - start));
        target[p - start] = '\0';
        return target;
    }
    if (*p == '<') {
        *angle = 1;
        const char *start = ++p;
        while (*p && *p != '>') p++;
        if (*p != '>') return NULL;
        char *target = malloc((size_t)(p - start) + 1);
        memcpy(target, start, (size_t)(p - start));
        target[p - start] = '\0';
        return target;
    }
    return NULL;
}

/* ---- include resolution -------------------------------------------------
 * Quote form: includer's own dir, then -I dirs, then the path as-is.
 * Angle form: -I dirs only, like a real compiler's system search --
 * deliberately NOT also "next to the includer", since angle form
 * means "library header" and this project's libraries live on the -I
 * path. The bare-path last resort for quote form covers running
 * v32c++ from a directory containing headers referenced by name. */
static char *resolve_include(const char *target, int angle, const char *including_file,
                              char *const *dirs, int ndirs) {
    if (!angle) {
        char *dir = path_dirname(including_file);
        char *candidate = join_path(dir, target);
        free(dir);
        if (access(candidate, R_OK) == 0) return candidate;
        free(candidate);
    }
    for (int i = 0; i < ndirs; i++) {
        char *candidate = join_path(dirs[i], target);
        if (access(candidate, R_OK) == 0) return candidate;
        free(candidate);
    }
    if (!angle && access(target, R_OK) == 0) return strdup(target);
    return NULL;
}

/* ---- the pre-scan itself ------------------------------------------------ */

static PathSet g_once_set;    /* canonical paths of #pragma once files */
static PathSet g_stack;       /* canonical paths currently being expanded */
static PathSet g_interned;    /* see prescan_intern_filename */

const char *prescan_intern_filename(const char *filename) {
    for (int i = 0; i < g_interned.count; i++)
        if (strcmp(g_interned.items[i], filename) == 0)
            return g_interned.items[i];
    path_set_add(&g_interned, filename);
    return g_interned.items[g_interned.count - 1];
}

static void emit_marker(FILE *out, int lineno, const char *file) {
    /* GCC-style line marker, consumed by lexer.l's marker rule to
     * re-target g_lex_lineno/g_current_filename. Written onto its own
     * full line (own trailing newline) so that rule's pattern --
     * which includes the newline -- always matches it whole. */
    fprintf(out, "# %d \"%s\"\n", lineno, file);
}

/* Expands one file into `out`. Returns 0 on success, -1 on any error
 * (message already printed, mentioning file and line). */
static int expand_file(const char *path, FILE *out,
                       char *const *dirs, int ndirs, int is_top) {
    FILE *in = fopen(path, "r");
    if (in == NULL) {
        if (is_top)
            fprintf(stderr, "---- error: cannot open input file '%s': %s ----\n",
                    path, strerror(errno));
        return -1;
    }

    char *canonical = canonical_path(path);
    if (path_set_contains(&g_once_set, canonical)) {
        /* #pragma once already satisfied by an earlier inclusion of
         * this same file -- emit nothing, exactly like an #ifndef
         * guard that already fired. */
        free(canonical);
        fclose(in);
        return 0;
    }
    if (path_set_contains(&g_stack, canonical)) {
        fprintf(stderr, "---- error: #include cycle detected: '%s' is already being expanded ----\n",
                path);
        fprintf(stderr, "  (add #pragma once to the header, or break the cycle)\n");
        free(canonical);
        fclose(in);
        return -1;
    }
    path_set_add(&g_stack, canonical);
    free(canonical);

    emit_marker(out, 1, path);

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int lineno = 0;
    int in_block_comment = 0;
    int rc = 0;

    while ((len = getline(&line, &cap, in)) != -1) {
        lineno++;
        /* strip the newline getline kept, re-add on emit: normalizes a
         * final line without one so a marker can never end up glued to
         * the last content line of a file. */
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        const char *hash = find_directive(line, &in_block_comment);
        if (hash == NULL) {
            fprintf(out, "%s\n", line);
            continue;
        }

        const char *rest;
        char *word = directive_word(hash, &rest);

        if (strcmp(word, "include") == 0) {
            int angle;
            char *target = include_target(rest, &angle);
            if (target != NULL && is_expanded_extension(target)) {
                char *resolved = resolve_include(target, angle, path, dirs, ndirs);
                if (resolved == NULL) {
                    fprintf(stderr, "---- error: cannot find #include %s%s%s (from %s:%d) ----\n",
                            angle ? "<" : "\"", target, angle ? ">" : "\"", path, lineno);
                    rc = -1;
                    free(target);
                    free(word);
                    break;
                }
                rc = expand_file(resolved, out, dirs, ndirs, 0);
                /* Re-sync to THIS file's next line before continuing --
                 * the only line-number desync source left after this. */
                emit_marker(out, lineno + 1, path);
                free(resolved);
                free(target);
                free(word);
                if (rc != 0) break;
                continue;
            }
            /* .h / system includes and anything unrecognized: verbatim,
             * so lexer.l's existing pass-through sees them unchanged. */
            free(target);
        }
        else if (strcmp(word, "pragma") == 0) {
            const char *p = rest;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "once", 4) == 0 &&
                (p[4] == '\0' || p[4] == ' ' || p[4] == '\t')) {
                canonical = canonical_path(path);
                path_set_add(&g_once_set, canonical);
                free(canonical);
                free(word);
                /* dropped line: re-sync this file's line numbering */
                emit_marker(out, lineno + 1, path);
                continue;
            }
            /* any OTHER #pragma falls through to the verbatim emit below */
        }

        free(word);
        fprintf(out, "%s\n", line); /* generic verbatim pass-through */
    }

    free(line);
    fclose(in);
    path_set_pop(&g_stack);
    return rc;
}

FILE *prescan_expand(const char *input_filename, char *const *include_dirs, int include_dir_count) {
    FILE *out = tmpfile();
    if (out == NULL) {
        perror("tmpfile");
        return NULL;
    }
    if (expand_file(input_filename, out, include_dirs, include_dir_count, 1) != 0) {
        fclose(out);
        return NULL;
    }
    rewind(out);
    return out;
}
