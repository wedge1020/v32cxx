#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> /* access() */
#include "prescan.h"
#include "compat.h"
#include "generic.h"
#include "stdstring.h"
#include "macro.h"

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
    const char *slash = compat_last_separator(path);
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
    /* "." + "src/a.cpp" is just "src/a.cpp": the name shows up in
     * diagnostics and debug maps, so keep it as the person would write it */
    if (strcmp(dir, ".") == 0) return strdup(target);
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
    char *resolved = compat_realpath(path);
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
 * Quote form: includer's own dir, then -I dirs, then the path as-is
 * (relative to the current directory), then the system dirs.
 * Angle form: -I dirs, then the system dirs -- deliberately NOT also
 * "next to the includer", since angle form means "library header".
 * The system dirs ($V32CXX_INCLUDE, then V32CXX_INCLUDE_PATH -- see
 * config.h) come LAST in both forms so a project-local copy of a header
 * always wins over an installed one. */
static char *const *g_system_dirs = NULL;
static int g_system_dir_count = 0;

static char *search_dirs(const char *target, char *const *dirs, int ndirs) {
    for (int i = 0; i < ndirs; i++) {
        char *candidate = join_path(dirs[i], target);
        if (access(candidate, R_OK) == 0) return candidate;
        free(candidate);
    }
    return NULL;
}

static char *resolve_include(const char *target, int angle, const char *including_file,
                              char *const *dirs, int ndirs) {
    char *found;
    if (!angle) {
        char *dir = path_dirname(including_file);
        char *candidate = join_path(dir, target);
        free(dir);
        if (access(candidate, R_OK) == 0) return candidate;
        free(candidate);
    }
    if ((found = search_dirs(target, dirs, ndirs)) != NULL) return found;
    if (!angle && access(target, R_OK) == 0) return strdup(target);
    return search_dirs(target, g_system_dirs, g_system_dir_count);
}

/* ---- the pre-scan itself ------------------------------------------------ */

static PathSet g_once_set;    /* canonical paths of #pragma once files */
static PathSet g_stack;       /* canonical paths currently being expanded */
static PathSet g_interned;    /* see prescan_intern_filename */
/* Files whose #include line was already EMITTED verbatim once
 * this run -- a second include of the same file (by any path
 * spelling) drops the line instead. g_once_set already does
 * exactly this job for #pragma-once .hpp/.cpp expansions; this
 * extends the same canonical-path discipline to the lines that
 * pass through instead of expanding. */
static PathSet g_verbatim_set;   /* same struct family as g_once_set */

/* INVARIANT: every set above is static, zero-initialized, whole-run state
 * that only grows -- nothing here is ever reset, so prescan_expand() must
 * be called AT MOST ONCE per process. A second call would inherit stale
 * g_once_set/g_verbatim_set/g_interned entries and wrongly suppress
 * legitimate re-inclusion (g_stack alone self-cleans via path_set_pop).
 * If multi-file transpiles in one process ever arrive, each set needs an
 * explicit reset at the top of prescan_expand -- added then, not now. */


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

/* ---- logical lines ------------------------------------------------------
 * Physical lines ending in a backslash are spliced onto the next one
 * before anything else looks at them (translation phase 2), so a long
 * #define can be continued the usual way. One line of pushback lets the
 * macro expander peek ahead when a function-like invocation's arguments
 * continue onto the following line. */
typedef struct LineReader {
    FILE *in;
    int lineno;          /* physical lines consumed so far */
    char *pushed;        /* pushed-back logical line, or NULL */
    int pushed_nphys;
    int pushed_first;
} LineReader;

/* Returns a malloc'd logical line (no trailing newline) or NULL at EOF;
 * *first = its first physical line number, *nphys = physical lines used. */
static char *read_logical(LineReader *r, int *first, int *nphys) {
    if (r->pushed) {
        char *l = r->pushed;
        r->pushed = NULL;
        *first = r->pushed_first;
        *nphys = r->pushed_nphys;
        return l;
    }
    char *line = NULL;
    size_t cap = 0;
    long len = compat_getline(&line, &cap, r->in);
    if (len == -1) { free(line); return NULL; }
    *first = ++r->lineno;
    *nphys = 1;
    for (;;) {
        /* strip the newline getline kept, so a marker can never end up
         * glued to the last line of a file that lacks a final newline */
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        size_t chk = (size_t)len;
        while (chk > 0 && line[chk - 1] == '\r') chk--;
        if (chk == 0 || line[chk - 1] != '\\') break;
        line[chk - 1] = '\0';                /* splice: drop "\\" (+ any \r) */
        len = (long)(chk - 1);
        char *next = NULL;
        size_t ncap = 0;
        long nlen = compat_getline(&next, &ncap, r->in);
        if (nlen == -1) { free(next); break; }
        r->lineno++;
        (*nphys)++;
        line = realloc(line, (size_t)len + (size_t)nlen + 1);
        memcpy(line + len, next, (size_t)nlen + 1);
        len += nlen;
        free(next);
    }
    return line;
}

static void unread_logical(LineReader *r, char *line, int first, int nphys) {
    r->pushed = line;
    r->pushed_first = first;
    r->pushed_nphys = nphys;
}

/* ---- conditional stack -------------------------------------------------- */

typedef struct Cond {
    int parent_active;   /* was the enclosing region active? */
    int active;          /* is the current branch active? */
    int taken;           /* has any branch of this group been taken? */
    int seen_else;
    int line;            /* the opening #if's line, for diagnostics */
} Cond;

typedef struct CondStack {
    Cond *items;
    int count;
    int cap;
} CondStack;

static int cond_active(const CondStack *c) {
    return c->count == 0 || c->items[c->count - 1].active;
}

static void cond_push(CondStack *c, int value, int line) {
    if (c->count == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 8;
        c->items = realloc(c->items, sizeof(Cond) * (size_t)c->cap);
    }
    int parent = cond_active(c);
    Cond *n = &c->items[c->count++];
    n->parent_active = parent;
    n->active = parent && value;
    n->taken = n->active;
    n->seen_else = 0;
    n->line = line;
}

/* The single identifier operand of #ifdef/#ifndef/#undef; malloc'd, or
 * NULL when missing/malformed. Trailing comments are allowed. */
static char *directive_ident(const char *rest) {
    const char *p = rest;
    while (*p == ' ' || *p == '\t') p++;
    if (!macro_is_ident_start((unsigned char)*p)) return NULL;
    const char *st = p;
    while (macro_is_ident_char((unsigned char)*p)) p++;
    char *name = malloc((size_t)(p - st) + 1);
    memcpy(name, st, (size_t)(p - st));
    name[p - st] = '\0';
    return name;
}

static void emit_blank_lines(FILE *out, int n) {
    while (n-- > 0) fputc('\n', out);
}

/* Command-line -D/-U, applied in order at the start of the run. */
typedef struct CmdlineMacro { char *spec; int is_undef; } CmdlineMacro;
static CmdlineMacro *g_cmdline_macros = NULL;
static int g_cmdline_macro_count = 0;

void prescan_add_cmdline_macro(const char *spec, int is_undef) {
    g_cmdline_macros = realloc(g_cmdline_macros,
                               sizeof(CmdlineMacro) * (size_t)(g_cmdline_macro_count + 1));
    g_cmdline_macros[g_cmdline_macro_count].spec = strdup(spec);
    g_cmdline_macros[g_cmdline_macro_count].is_undef = is_undef;
    g_cmdline_macro_count++;
}

/* Expands one code line (plus however many following lines a multi-line
 * function-like invocation needs) and writes it out. Returns 0 or -1. */
static int emit_code_line(FILE *out, LineReader *r, char *line, int first, int nphys,
                          int state_before, int *in_block_comment, const char *path) {
    if (macro_count() == 0 && strstr(line, "__") == NULL) {
        fprintf(out, "%s\n", line);
        free(line);
        return 0;
    }
    char *buf = line;
    int total_phys = nphys;
    char *expanded = NULL;
    for (;;) {
        int rc = macro_expand_text(buf, state_before, 1, path, first, &expanded);
        if (rc == MACRO_OK) break;
        if (rc == MACRO_ERROR) { free(buf); return -1; }
        /* MACRO_NEED_MORE: pull in the next line unless it's a directive
         * (or EOF), in which case the invocation simply isn't one. */
        int nfirst, nn;
        char *next = read_logical(r, &nfirst, &nn);
        int saved = *in_block_comment;
        if (next != NULL && find_directive(next, in_block_comment) != NULL) {
            *in_block_comment = saved;
            unread_logical(r, next, nfirst, nn);
            next = NULL;
        }
        if (next == NULL) {
            rc = macro_expand_text(buf, state_before, 0, path, first, &expanded);
            if (rc != MACRO_OK) { free(buf); return -1; }
            break;
        }
        size_t a = strlen(buf), b = strlen(next);
        buf = realloc(buf, a + b + 2);
        buf[a] = '\n';
        memcpy(buf + a + 1, next, b + 1);
        free(next);
        total_phys += nn;
    }
    fprintf(out, "%s\n", expanded);
    free(expanded);
    free(buf);
    if (total_phys > 1) emit_marker(out, first + total_phys, path);
    return 0;
}

/* ---- pass-through header harvesting ---------------------------------------
 * A .h header passes through to the generated C (the Vircon32 C compiler
 * includes it), but v32c++ ALSO reads it when it can find it -- in
 * "harvest" mode -- so the C++ side sees what it defines:
 *   - its #define'd macros (screen_width, color_red, pi, ...) become
 *     ordinary macros here: usable in array sizes and #if, typed for
 *     overload resolution, and named constants keep their names in the
 *     output. They are NOT re-emitted -- the header itself still defines
 *     them downstream;
 *   - its struct and typedef names (date_info, game_signature, ...) are
 *     declared `native` automatically, right after the #include line, so
 *     source can use them by pointer without writing `native` itself.
 * Only directives are processed; code is skipped, so nothing in the header
 * has to be C++ v32c++ can parse. Lookup: the normal include search, then
 * the SDK include directories (see config.h). An unfound header is
 * skipped silently, exactly as before. */
static char *const *g_sdk_dirs = NULL;
static int g_sdk_dir_count = 0;

void prescan_set_sdk_dirs(char *const *dirs, int count) {
    g_sdk_dirs = dirs;
    g_sdk_dir_count = count;
}

static PathSet g_harvest_types;     /* struct/typedef names found this harvest */

/* Records `struct NAME` (a definition: `{` follows, or nothing more on the
 * line) and `typedef ... NAME;` found on a header's code line. Comment-
 * and string-free enough for SDK headers; anything odder is just missed. */
static void harvest_type_names(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "struct", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        p += 6;
        while (*p == ' ' || *p == '\t') p++;
        const char *st = p;
        while (macro_is_ident_char((unsigned char)*p)) p++;
        if (p == st) return;
        const char *q = p;
        while (*q == ' ' || *q == '\t' || *q == '\r') q++;
        if (*q != '{' && *q != '\0' && strncmp(q, "//", 2) != 0) return;   /* a use, not a definition */
        char name[128];
        size_t n = (size_t)(p - st) < sizeof(name) - 1 ? (size_t)(p - st) : sizeof(name) - 1;
        memcpy(name, st, n);
        name[n] = '\0';
        if (!path_set_contains(&g_harvest_types, name)) path_set_add(&g_harvest_types, name);
        return;
    }
    if (strncmp(p, "typedef", 7) == 0 && (p[7] == ' ' || p[7] == '\t')) {
        const char *semi = strchr(p, ';');
        if (semi == NULL) return;
        const char *e = semi;
        while (e > p && (e[-1] == ' ' || e[-1] == '\t')) e--;
        const char *st = e;
        while (st > p && macro_is_ident_char((unsigned char)st[-1])) st--;
        if (st == e || !macro_is_ident_start((unsigned char)*st)) return;
        char name[128];
        size_t n = (size_t)(e - st) < sizeof(name) - 1 ? (size_t)(e - st) : sizeof(name) - 1;
        memcpy(name, st, n);
        name[n] = '\0';
        if (!path_set_contains(&g_harvest_types, name)) path_set_add(&g_harvest_types, name);
    }
}

/* resolve_include, then the SDK directories (pass-through .h only). */
static char *resolve_header(const char *target, int angle, const char *including_file,
                            char *const *dirs, int ndirs) {
    char *found = resolve_include(target, angle, including_file, dirs, ndirs);
    if (found == NULL) found = search_dirs(target, g_sdk_dirs, g_sdk_dir_count);
    return found;
}

/* Expands one file into `out`. Returns 0 on success, -1 on any error
 * (message already printed, mentioning file and line). With `harvest`
 * set, only reads it for macros and type names (see above): `out` is
 * NULL and nothing is written. */
static int expand_file_mode(const char *path, FILE *out,
                            char *const *dirs, int ndirs, int is_top, int harvest);

static int expand_file(const char *path, FILE *out,
                       char *const *dirs, int ndirs, int is_top) {
    return expand_file_mode(path, out, dirs, ndirs, is_top, 0);
}

/* The name the built-in <string> header goes by: in diagnostics, in line
 * markers, and as its #pragma once key. Not a real path. */
#define BUILTIN_STRING_PATH "<string>"

/* The built-in header's text in a temporary file, ready to read. */
static FILE *open_builtin_string(void) {
    FILE *f = compat_tmpfile();
    if (f == NULL) return NULL;
    for (int i = 0; g_stdstring_lines[i] != NULL; i++) {
        fputs(g_stdstring_lines[i], f);
        fputc('\n', f);
    }
    rewind(f);
    return f;
}

static int expand_file_mode(const char *path, FILE *out,
                            char *const *dirs, int ndirs, int is_top, int harvest) {
    FILE *in = (strcmp(path, BUILTIN_STRING_PATH) == 0) ? open_builtin_string() : fopen(path, "r");
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

    /* Interned: macros defined in this file keep pointing at the name. */
    path = prescan_intern_filename(path);
    if (!harvest) emit_marker(out, 1, path);

    LineReader reader = { in, 0, NULL, 0, 0 };
    CondStack conds = { NULL, 0, 0 };
    char *line;
    int first, nphys;
    int in_block_comment = 0;
    int rc = 0;

    while (rc == 0 && (line = read_logical(&reader, &first, &nphys)) != NULL) {
        int lineno = first;
        int state_before = in_block_comment;
        const char *hash = find_directive(line, &in_block_comment);
        int active = cond_active(&conds);

        if (hash == NULL) {
            if (harvest) {
                if (active && !state_before) harvest_type_names(line);
                free(line);
                continue;
            }
            if (!active) {
                emit_blank_lines(out, nphys);
                free(line);
                continue;
            }
            rc = emit_code_line(out, &reader, line, first, nphys, state_before,
                                &in_block_comment, path);
            continue;
        }

        const char *rest;
        char *word = directive_word(hash, &rest);
        int consumed = 1;       /* default: the line does not reach the output */
        char *emit_text = NULL; /* what reaches it otherwise (NULL: `line`) */
        int extra_lines = 0;    /* lines emit_text adds beyond the one it replaces */

        /* ---- conditionals: processed even inside inactive regions ---- */
        if (strcmp(word, "ifdef") == 0 || strcmp(word, "ifndef") == 0) {
            int value = 0;
            if (cond_active(&conds)) {
                char *name = directive_ident(rest);
                if (name == NULL) {
                    fprintf(stderr, "%s:%d: error: #%s expects a macro name\n", path, lineno, word);
                    rc = -1;
                } else {
                    int defined = macro_lookup(name) != NULL ||
                                  strcmp(name, "__FILE__") == 0 || strcmp(name, "__LINE__") == 0;
                    value = (word[2] == 'd') ? defined : !defined;
                    free(name);
                }
            }
            cond_push(&conds, value, lineno);
        }
        else if (strcmp(word, "if") == 0) {
            long value = 0;
            if (cond_active(&conds) && macro_eval_condition(rest, path, lineno, &value) != 0)
                rc = -1;
            cond_push(&conds, value != 0, lineno);
        }
        else if (strcmp(word, "elif") == 0 || strcmp(word, "else") == 0) {
            if (conds.count == 0) {
                fprintf(stderr, "%s:%d: error: #%s without #if\n", path, lineno, word);
                rc = -1;
            } else {
                Cond *c = &conds.items[conds.count - 1];
                if (c->seen_else) {
                    fprintf(stderr, "%s:%d: error: #%s after #else\n", path, lineno, word);
                    rc = -1;
                } else if (word[1] == 'l' && word[2] == 's') {        /* else */
                    c->seen_else = 1;
                    c->active = c->parent_active && !c->taken;
                    c->taken = c->taken || c->active;
                } else {                                              /* elif */
                    long value = 0;
                    if (c->parent_active && !c->taken &&
                        macro_eval_condition(rest, path, lineno, &value) != 0)
                        rc = -1;
                    c->active = c->parent_active && !c->taken && value != 0;
                    c->taken = c->taken || c->active;
                }
            }
        }
        else if (strcmp(word, "endif") == 0) {
            if (conds.count == 0) {
                fprintf(stderr, "%s:%d: error: #endif without #if\n", path, lineno);
                rc = -1;
            } else {
                conds.count--;
            }
        }
        /* ---- everything else only matters in an active region ---- */
        else if (!active) {
            /* dropped */
        }
        else if (strcmp(word, "define") == 0) {
            Macro *m = macro_define(rest, path, lineno);
            if (m == NULL) {
                rc = -1;
            } else if (harvest) {
                /* the header defines it downstream itself */
            } else if (macro_passthrough_ok(m)) {
                /* Pass the definition through to the generated C too, so
                 * the name still exists downstream (for pass-through .h
                 * headers, and for whoever reads the output). Verbatim
                 * when it was one physical line; rebuilt, comments
                 * stripped, when it was continued with backslashes. */
                if (nphys > 1) emit_text = macro_format_define(m);
                consumed = 0;
            }
            /* else: # / ## / variadic / self-referencing bodies are hard
             * errors for the Vircon32 C preprocessor -- every use has
             * already been expanded here, so the line is just dropped. */
        }
        else if (strcmp(word, "undef") == 0) {
            char *name = directive_ident(rest);
            if (name == NULL) {
                fprintf(stderr, "%s:%d: error: #undef expects a macro name\n", path, lineno);
                rc = -1;
            } else {
                macro_undef(name);
                free(name);
                consumed = 0;   /* harmless downstream either way */
            }
        }
        else if (strcmp(word, "error") == 0 || strcmp(word, "warning") == 0) {
            const char *msg = rest;
            while (*msg == ' ' || *msg == '\t') msg++;
            fprintf(stderr, "%s:%d: %s: #%s %s\n", path, lineno,
                    word[0] == 'e' ? "error" : "warning", word, msg);
            if (word[0] == 'e') rc = -1;
        }
        else if (strcmp(word, "include") == 0) {
            int angle;
            char *target = include_target(rest, &angle);
            if (target != NULL && angle && strcmp(target, "array") == 0) {
                /* `#include <array>`: there is no such file. It switches
                 * on the transpiler's own std::array (see generic.h) and
                 * must never reach the generated C. */
                g_generic_array_enabled = 1;
                emit_marker(out, lineno + nphys, path);
                free(target);
                free(word);
                free(line);
                continue;
            }
            else if (target != NULL && angle && strcmp(target, "vector") == 0) {
                /* `#include <vector>`: the same, for std::vector. */
                g_generic_vector_enabled = 1;
                emit_marker(out, lineno + nphys, path);
                free(target);
                free(word);
                free(line);
                continue;
            }
            else if (target != NULL && angle && strcmp(target, "string") == 0) {
                /* `#include <string>`: std::string, from the header built
                 * into the transpiler (stdstring.h). Nothing reaches the
                 * generated C but the class itself. */
                if (!harvest) {
                    rc = expand_file(BUILTIN_STRING_PATH, out, dirs, ndirs, 0);
                    emit_marker(out, lineno + nphys, path);
                }
                free(target);
                free(word);
                free(line);
                continue;
            }
            else if (target != NULL && is_expanded_extension(target) && harvest) {
                /* a .hpp included from a pass-through .h: not ours to read */
                free(target);
            }
            else if (target != NULL && is_expanded_extension(target)) {
                char *resolved = resolve_include(target, angle, path, dirs, ndirs);
                if (resolved == NULL) {
                    fprintf(stderr, "---- error: cannot find #include %s%s%s (from %s:%d) ----\n",
                            angle ? "<" : "\"", target, angle ? ">" : "\"", path, lineno);
                    rc = -1;
                } else {
                    rc = expand_file(resolved, out, dirs, ndirs, 0);
                    /* Re-sync to THIS file's next line before continuing. */
                    emit_marker(out, lineno + nphys, path);
                    free(resolved);
                }
                free(target);
                free(word);
                free(line);
                continue;
            }
            else if (target != NULL) {
                /* A non-expanding include (the SDK's .h headers, or anything
                 * the prescan doesn't own): resolve it the same way the
                 * expanding branch does, so dedupe keys on the CANONICAL
                 * path -- never on the raw spelling, which two headers can
                 * write differently while naming the same file. When the
                 * same file was already emitted once (verbatim) this run,
                 * drop the line entirely: lexer.l's pass-through would
                 * otherwise re-emit it at the top of the generated C.
                 * An unresolvable one is NOT an error -- the downstream
                 * Vircon32 C compiler owns those paths (SDK headers in its
                 * own include dir) -- and passes through verbatim. */
                char *resolved = resolve_header(target, angle, path, dirs, ndirs);
                int duplicate = 0;
                if (resolved != NULL) {
                    char *canon = canonical_path(resolved);
                    if (path_set_contains(&g_verbatim_set, canon)) duplicate = 1;
                    else path_set_add(&g_verbatim_set, canon);
                    free(canon);
                    if (!duplicate) {
                        /* read it for its macros and type names */
                        int before = g_harvest_types.count;
                        rc = expand_file_mode(resolved, NULL, dirs, ndirs, 0, 1);
                        if (!harvest && g_harvest_types.count > before) {
                            /* `native NAME;` for each new struct/typedef,
                             * on lines right after the #include itself */
                            size_t len = strlen(line) + 1;
                            for (int t = before; t < g_harvest_types.count; t++)
                                len += strlen(g_harvest_types.items[t]) + 10;
                            emit_text = malloc(len);
                            strcpy(emit_text, line);
                            for (int t = before; t < g_harvest_types.count; t++) {
                                strcat(emit_text, "\nnative ");
                                strcat(emit_text, g_harvest_types.items[t]);
                                strcat(emit_text, ";");
                                extra_lines++;
                            }
                        }
                    }
                    free(resolved);
                }
                free(target);
                if (!duplicate) consumed = 0;
            }
            else {
                consumed = 0;   /* unrecognized shape: verbatim, as always */
            }
        }
        else if (strcmp(word, "pragma") == 0) {
            const char *p = rest;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "once", 4) == 0 &&
                (p[4] == '\0' || p[4] == ' ' || p[4] == '\t' || p[4] == '\r')) {
                canonical = canonical_path(path);
                path_set_add(&g_once_set, canonical);
                free(canonical);
            } else if (!harvest) {
                /* The Vircon32 C preprocessor rejects every #pragma as an
                 * unsupported directive, so passing one through could only
                 * break the downstream compile. */
                fprintf(stderr, "%s:%d: warning: ignoring '#pragma %s' (not supported by Vircon32 C)\n",
                        path, lineno, p);
            }
        }
        else if (word[0] == '\0') {
            /* the null directive: a lone '#' */
        }
        else {
            /* #texture / #sound / #title / #version cart hints (lexer.l
             * recognizes those), and anything else: verbatim, as always. */
            consumed = 0;
        }

        if (harvest) {
            /* nothing reaches the stream */
        } else if (consumed) {
            emit_blank_lines(out, nphys);
        } else {
            fprintf(out, "%s\n", emit_text ? emit_text : line);
            if (nphys > 1 || extra_lines > 0) emit_marker(out, lineno + nphys, path);
        }
        free(emit_text);
        free(word);
        free(line);
    }

    if (rc == 0 && conds.count > 0) {
        fprintf(stderr, "%s:%d: error: unterminated #%s (opened here, never closed by #endif)\n",
                path, conds.items[conds.count - 1].line, "if");
        rc = -1;
    }
    free(conds.items);
    free(reader.pushed);
    fclose(in);
    path_set_pop(&g_stack);
    return rc;
}

FILE *prescan_expand(const char *input_filename, char *const *include_dirs, int include_dir_count,
                     char *const *system_dirs, int system_dir_count) {
    g_system_dirs = system_dirs;
    g_system_dir_count = system_dir_count;
    FILE *out = compat_tmpfile();
    if (out == NULL) {
        perror("tmpfile");
        return NULL;
    }
    /* -D / -U, in command-line order. A -D definition reaches the
     * generated C like any other #define (written ahead of the first
     * line marker, so it costs no source line). */
    for (int i = 0; i < g_cmdline_macro_count; i++) {
        if (g_cmdline_macros[i].is_undef) {
            macro_undef(g_cmdline_macros[i].spec);
            continue;
        }
        Macro *m = macro_define_cmdline(g_cmdline_macros[i].spec);
        if (m == NULL) { fclose(out); return NULL; }
        if (macro_passthrough_ok(m)) {
            char *def = macro_format_define(m);
            fprintf(out, "%s\n", def);
            free(def);
        }
    }
    if (expand_file(input_filename, out, include_dirs, include_dir_count, 1) != 0) {
        fclose(out);
        return NULL;
    }
    rewind(out);
    return out;
}
