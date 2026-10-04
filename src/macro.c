/*
 * macro.c -- the C++-side macro table, expander and #if evaluator used by
 * the include pre-scan. See macro.h for the overview.
 *
 * The expander works on one text buffer at a time (a source line, or a
 * few joined lines when a function-like invocation spans them), splicing
 * each replacement into the buffer in place and then rescanning from the
 * start of the replacement -- which is what lets an expansion combine
 * with the text that FOLLOWS it (`#define CALL f` then `CALL(1)`).
 *
 * Self-reference protection uses region-based "hide sets": every splice
 * records (macro name, end offset of its replacement), and a name found
 * at an offset inside a live region of the same name is left alone. That
 * is the classic approximation of the standard's per-token hide sets and
 * agrees with it on everything short of contrived cases.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "macro.h"
#include "config.h"

/* ---- growable text buffer ---------------------------------------------- */

typedef struct Buf {
    char *s;
    size_t len;
    size_t cap;
} Buf;

static void buf_reserve(Buf *b, size_t extra) {
    if (b->len + extra + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (b->len + extra + 1 > cap) cap *= 2;
        b->s = realloc(b->s, cap);
        b->cap = cap;
    }
}

static void buf_putn(Buf *b, const char *s, size_t n) {
    buf_reserve(b, n);
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}

static void buf_puts(Buf *b, const char *s) { buf_putn(b, s, strlen(s)); }
static void buf_putc(Buf *b, char c) { buf_putn(b, &c, 1); }

static void buf_init_from(Buf *b, const char *s) {
    b->s = NULL; b->len = 0; b->cap = 0;
    buf_puts(b, s ? s : "");
}

/* Replaces [start, end) with `repl`. */
static void buf_splice(Buf *b, size_t start, size_t end, const char *repl, size_t rlen) {
    size_t tail = b->len - end;
    if (rlen > end - start) buf_reserve(b, rlen - (end - start));
    memmove(b->s + start + rlen, b->s + end, tail + 1); /* + the NUL */
    memcpy(b->s + start, repl, rlen);
    b->len = start + rlen + tail;
}

static void buf_rtrim(Buf *b) {
    while (b->len > 0 && isspace((unsigned char)b->s[b->len - 1])) b->s[--b->len] = '\0';
}

/* ---- character classes / small scanners -------------------------------- */

int macro_is_ident_start(int c) { return isalpha(c) || c == '_'; }
int macro_is_ident_char(int c)  { return isalnum(c) || c == '_'; }

/* Index just past the string or character literal starting at s[i]
 * (s[i] is the opening quote). Backslash escapes honored; an
 * unterminated literal stops at the newline (the lexer reports it). */
static size_t skip_literal(const char *s, size_t i, size_t len) {
    char q = s[i++];
    while (i < len && s[i] != '\n') {
        if (s[i] == '\\' && i + 1 < len) { i += 2; continue; }
        if (s[i] == q) return i + 1;
        i++;
    }
    return i;
}

/* Index just past the preprocessing number starting at s[i] -- digits,
 * letters, '_', '.', and e+/e-/p+/p- exponents -- so the "e5" of 1e5 or
 * the "x1F" of 0x1F is never mistaken for an identifier. */
static size_t skip_ppnumber(const char *s, size_t i, size_t len) {
    i++;
    while (i < len) {
        char c = s[i];
        if ((c == '+' || c == '-') && strchr("eEpP", s[i - 1])) { i++; continue; }
        if (macro_is_ident_char((unsigned char)c) || c == '.') { i++; continue; }
        break;
    }
    return i;
}

/* Skips whitespace (newlines included) and comments from s[i]. Returns the
 * index of the next real character, or len when only whitespace/comments
 * remain. An unterminated block comment also runs to len. */
static size_t skip_space_and_comments(const char *s, size_t i, size_t len) {
    while (i < len) {
        if (isspace((unsigned char)s[i])) { i++; continue; }
        if (s[i] == '/' && i + 1 < len && s[i + 1] == '/') {
            while (i < len && s[i] != '\n') i++;
            continue;
        }
        if (s[i] == '/' && i + 1 < len && s[i + 1] == '*') {
            const char *close = strstr(s + i + 2, "*/");
            if (close == NULL) return len;
            i = (size_t)(close - s) + 2;
            continue;
        }
        break;
    }
    return i;
}

/* Would `a` immediately followed by `b` lex as one token where two were
 * meant? Used wherever replacement text meets its neighbors, so that
 * `-NEG` with NEG = -1 becomes `- -1`, never the decrement `--1`. */
static int would_glue(char a, char b) {
    if (a == '\0' || b == '\0') return 0;
    if (isspace((unsigned char)a) || isspace((unsigned char)b)) return 0;
    if (macro_is_ident_char((unsigned char)a) && macro_is_ident_char((unsigned char)b)) return 1;
    if (macro_is_ident_char((unsigned char)a) && (b == '"' || b == '\'')) return 1;
    const char *punct = "+-*/%<>=!&|^~.:#";
    if (strchr(punct, a) && strchr(punct, b)) return 1;
    if (a == '.' && isdigit((unsigned char)b)) return 1;
    return 0;
}

static char *dup_range(const char *s, size_t start, size_t end) {
    char *r = malloc(end - start + 1);
    memcpy(r, s + start, end - start);
    r[end - start] = '\0';
    return r;
}

static char *dup_trimmed(const char *s, size_t start, size_t end) {
    while (start < end && isspace((unsigned char)s[start])) start++;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;
    return dup_range(s, start, end);
}

/* Copies `text` with every comment replaced by one space (literals left
 * untouched), trimmed. Used for macro bodies, so a trailing `// note` on
 * a #define line can never swallow the code a use of it is spliced into. */
static char *strip_comments(const char *text) {
    Buf b;
    buf_init_from(&b, "");
    size_t len = strlen(text), i = 0;
    while (i < len) {
        if (text[i] == '"' || text[i] == '\'') {
            size_t e = skip_literal(text, i, len);
            buf_putn(&b, text + i, e - i);
            i = e;
        } else if (text[i] == '/' && i + 1 < len && text[i + 1] == '/') {
            break;
        } else if (text[i] == '/' && i + 1 < len && text[i + 1] == '*') {
            const char *close = strstr(text + i + 2, "*/");
            buf_putc(&b, ' ');
            if (close == NULL) break;
            i = (size_t)(close - text) + 2;
        } else {
            buf_putc(&b, text[i++]);
        }
    }
    char *r = dup_trimmed(b.s, 0, b.len);
    free(b.s);
    return r;
}

/* ---- the macro table ---------------------------------------------------- */

#define MACRO_BUCKETS 256
typedef struct MacroNode { Macro *m; struct MacroNode *next; } MacroNode;
static MacroNode *g_buckets[MACRO_BUCKETS];
static int g_macro_count = 0;
static int g_builtins_done = 0;

/* Names whose meaning changes somewhere in the run -- redefined with a
 * different body, or #undef'd. All #define/#undef lines reach the top of
 * the generated C in one block, so downstream only ever sees a name's
 * FINAL state; printing such a name back into the code could give it the
 * wrong value (or none). See macro_name_is_stable. */
static char **g_unstable = NULL;
static int g_unstable_count = 0;

static void mark_unstable(const char *name) {
    for (int i = 0; i < g_unstable_count; i++)
        if (strcmp(g_unstable[i], name) == 0) return;
    g_unstable = realloc(g_unstable, sizeof(char *) * (size_t)(g_unstable_count + 1));
    g_unstable[g_unstable_count++] = strdup(name);
}

static unsigned hash_name(const char *s) {
    unsigned h = 5381;
    while (*s) h = h * 33u + (unsigned char)*s++;
    return h % MACRO_BUCKETS;
}

static void macro_free(Macro *m) {
    for (int i = 0; i < m->param_count; i++) free(m->params[i]);
    free(m->params);
    free(m->name);
    free(m->body);
    free(m);
}

static Macro *table_find(const char *name, MacroNode ***link_out) {
    MacroNode **link = &g_buckets[hash_name(name)];
    while (*link) {
        if (strcmp((*link)->m->name, name) == 0) {
            if (link_out) *link_out = link;
            return (*link)->m;
        }
        link = &(*link)->next;
    }
    if (link_out) *link_out = link;
    return NULL;
}

static void table_remove(const char *name) {
    MacroNode **link;
    if (table_find(name, &link) != NULL) {
        MacroNode *dead = *link;
        *link = dead->next;
        macro_free(dead->m);
        free(dead);
        g_macro_count--;
    }
}

static void table_add(Macro *m) {
    table_remove(m->name);
    MacroNode *n = malloc(sizeof(MacroNode));
    n->m = m;
    n->next = g_buckets[hash_name(m->name)];
    g_buckets[hash_name(m->name)] = n;
    g_macro_count++;
}

/* __V32CXX__ lets source detect the transpiler (#ifdef __V32CXX__). It is
 * a builtin: never passed through, and is_builtin keeps it out of
 * macro_count() so a program with no #define of its own still skips
 * expansion entirely. __FILE__/__LINE__ are handled inside the expander. */
static int g_builtin_count = 0;
static void ensure_builtins(void) {
    if (g_builtins_done) return;
    g_builtins_done = 1;
    Macro *m = calloc(1, sizeof(Macro));
    m->name = strdup("__V32CXX__");
    m->body = strdup("1");
    m->file = "<built-in>";
    m->is_builtin = 1;
    table_add(m);
    /* NULL is a keyword of Vircon32 C, not a macro from some header, so
     * portable C's `#ifndef NULL / #define NULL 0 / #endif` would otherwise
     * take its fallback -- and that #define, passed through to the
     * output, turns every NULL in the generated C (the runtime helpers'
     * included) into an int 0 the real compiler refuses to compare with a
     * pointer. Predefined as itself: `#ifndef NULL` is false, and a use
     * of NULL expands to NULL. */
    m = calloc(1, sizeof(Macro));
    m->name = strdup("NULL");
    m->body = strdup("NULL");
    m->file = "<built-in>";
    m->is_builtin = 1;
    table_add(m);
    g_builtin_count = 2;
}

Macro *macro_lookup(const char *name) {
    ensure_builtins();
    return table_find(name, NULL);
}

int macro_count(void) {
    ensure_builtins();
    return g_macro_count - g_builtin_count;
}

void macro_undef(const char *name) {
    ensure_builtins();
    Macro *m = table_find(name, NULL);
    if (m != NULL && m->is_builtin) g_builtin_count--;
    if (m != NULL) mark_unstable(name);
    table_remove(name);
}

int macro_name_is_stable(const char *name) {
    for (int i = 0; i < g_unstable_count; i++)
        if (strcmp(g_unstable[i], name) == 0) return 0;
    return 1;
}

static int same_definition(const Macro *a, const Macro *b) {
    if (a->is_function != b->is_function || a->is_variadic != b->is_variadic ||
        a->param_count != b->param_count)
        return 0;
    for (int i = 0; i < a->param_count; i++)
        if (strcmp(a->params[i], b->params[i]) != 0) return 0;
    /* compare bodies with whitespace runs collapsed */
    const char *p = a->body, *q = b->body;
    while (*p || *q) {
        if (isspace((unsigned char)*p) && isspace((unsigned char)*q)) {
            while (isspace((unsigned char)*p)) p++;
            while (isspace((unsigned char)*q)) q++;
            continue;
        }
        if (*p != *q) return 0;
        p++; q++;
    }
    return 1;
}

static int param_index(const Macro *m, const char *name, size_t len) {
    for (int i = 0; i < m->param_count; i++)
        if (strlen(m->params[i]) == len && strncmp(m->params[i], name, len) == 0) return i;
    if (m->is_variadic && len == 11 && strncmp(name, "__VA_ARGS__", 11) == 0)
        return m->param_count;
    return -1;
}

/* Validates # / ## placement in a function-like body: # must precede a
 * parameter, ## may not start or end the body. */
static int check_body_operators(const Macro *m, const char *file, int line) {
    const char *b = m->body;
    size_t len = strlen(b);
    if (len >= 2 && strncmp(b, "##", 2) == 0) {
        fprintf(stderr, "%s:%d: error: '##' cannot appear at either end of a macro expansion\n", file, line);
        return 0;
    }
    if (len >= 2 && strncmp(b + len - 2, "##", 2) == 0) {
        fprintf(stderr, "%s:%d: error: '##' cannot appear at either end of a macro expansion\n", file, line);
        return 0;
    }
    if (!m->is_function) return 1;
    for (size_t i = 0; i < len; ) {
        if (b[i] == '"' || b[i] == '\'') { i = skip_literal(b, i, len); continue; }
        if (b[i] == '#' && i + 1 < len && b[i + 1] == '#') { i += 2; continue; }
        if (b[i] == '#') {
            size_t j = i + 1;
            while (j < len && isspace((unsigned char)b[j])) j++;
            size_t k = j;
            while (k < len && macro_is_ident_char((unsigned char)b[k])) k++;
            if (k == j || param_index(m, b + j, k - j) < 0) {
                fprintf(stderr, "%s:%d: error: '#' is not followed by a macro parameter\n", file, line);
                return 0;
            }
            i = k;
            continue;
        }
        i++;
    }
    return 1;
}

Macro *macro_define(const char *rest, const char *file, int line) {
    ensure_builtins();
    const char *p = rest;
    while (*p == ' ' || *p == '\t') p++;
    if (!macro_is_ident_start((unsigned char)*p)) {
        fprintf(stderr, "%s:%d: error: %s\n", file, line,
                *p ? "macro name must be an identifier" : "no macro name given in #define directive");
        return NULL;
    }
    const char *ns = p;
    while (macro_is_ident_char((unsigned char)*p)) p++;

    Macro *m = calloc(1, sizeof(Macro));
    m->name = dup_range(ns, 0, (size_t)(p - ns));
    m->file = file;
    m->line = line;

    if (strcmp(m->name, "defined") == 0) {
        fprintf(stderr, "%s:%d: error: \"defined\" cannot be used as a macro name\n", file, line);
        macro_free(m);
        return NULL;
    }

    if (*p == '(') {
        /* '(' IMMEDIATELY after the name (no space): function-like -- the
         * same rule real C and the Vircon32 C preprocessor both apply. */
        m->is_function = 1;
        p++;
        int cap = 0;
        for (;;) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == ')' && m->param_count == 0 && !m->is_variadic) { p++; break; }
            if (strncmp(p, "...", 3) == 0) {
                m->is_variadic = 1;
                p += 3;
                while (*p == ' ' || *p == '\t') p++;
                if (*p != ')') {
                    fprintf(stderr, "%s:%d: error: '...' must be the last macro parameter\n", file, line);
                    macro_free(m);
                    return NULL;
                }
                p++;
                break;
            }
            if (!macro_is_ident_start((unsigned char)*p)) {
                fprintf(stderr, "%s:%d: error: expected a parameter name in the definition of '%s'\n",
                        file, line, m->name);
                macro_free(m);
                return NULL;
            }
            const char *ps = p;
            while (macro_is_ident_char((unsigned char)*p)) p++;
            char *pname = dup_range(ps, 0, (size_t)(p - ps));
            if (param_index(m, pname, strlen(pname)) >= 0 || strcmp(pname, "__VA_ARGS__") == 0) {
                fprintf(stderr, "%s:%d: error: duplicate or reserved macro parameter '%s'\n",
                        file, line, pname);
                free(pname);
                macro_free(m);
                return NULL;
            }
            if (m->param_count == cap) {
                cap = cap ? cap * 2 : 4;
                m->params = realloc(m->params, sizeof(char *) * (size_t)cap);
            }
            m->params[m->param_count++] = pname;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == ',') { p++; continue; }
            if (*p == ')') { p++; break; }
            fprintf(stderr, "%s:%d: error: expected ',' or ')' in the parameter list of '%s'\n",
                    file, line, m->name);
            macro_free(m);
            return NULL;
        }
    } else if (*p != '\0' && *p != ' ' && *p != '\t' && *p != '/') {
        fprintf(stderr, "%s:%d: error: whitespace is required after the macro name '%s'\n",
                file, line, m->name);
        macro_free(m);
        return NULL;
    }

    m->body = strip_comments(p);
    if (!check_body_operators(m, file, line)) {
        macro_free(m);
        return NULL;
    }

    Macro *old = table_find(m->name, NULL);
    if (old != NULL && old->is_builtin) {
        g_builtin_count--;
    } else if (old != NULL && !same_definition(old, m)) {
        mark_unstable(m->name);
        fprintf(stderr, "%s:%d: warning: '%s' redefined (previous definition at %s:%d)\n",
                file, line, m->name, old->file, old->line);
    }
    table_add(m);
    return m;
}

Macro *macro_define_cmdline(const char *spec) {
    /* -D NAME=VALUE  ->  #define NAME VALUE ;  -D NAME  ->  #define NAME 1 */
    const char *eq = strchr(spec, '=');
    Buf b;
    buf_init_from(&b, "");
    if (eq) {
        buf_putn(&b, spec, (size_t)(eq - spec));
        buf_putc(&b, ' ');
        buf_puts(&b, eq + 1);
    } else {
        buf_puts(&b, spec);
        buf_puts(&b, " 1");
    }
    Macro *m = macro_define(b.s, "<command line>", 0);
    free(b.s);
    return m;
}

/* C input: every use of every macro has been expanded by the time the
 * generated C is written, so a #define is only passed through when it is
 * worth having there by name -- a constant: an object-like macro whose
 * body is literals, operators, and other macros that are passed through
 * themselves (`#define STATLINE (NUMLINES - 1)`). Everything else stays
 * out: C programs define macros Vircon32 C's preprocessor cannot take
 * (`#define when break;case`, a body with `?`), and an octal constant
 * (`#define ISHELD 0000400`) would be read as decimal there. */
extern int g_c_mode;

static int c_passthrough_ok(const Macro *m) {
    if (m->is_function) return 0;
    const char *b = m->body;
    size_t len = strlen(b);
    for (size_t i = 0; i < len; ) {
        unsigned char ch = (unsigned char)b[i];
        if (ch == '"' || ch == '\'') { i = skip_literal(b, i, len); continue; }
        if (macro_is_ident_start(ch)) {
            size_t j = i;
            while (j < len && macro_is_ident_char((unsigned char)b[j])) j++;
            char *name = dup_range(b, i, j);
            Macro *other = macro_lookup(name);
            free(name);
            if (other == NULL || other == m || !macro_passthrough_ok(other)) return 0;
            i = j;
            continue;
        }
        if (isdigit(ch)) {
            if (ch == '0' && isdigit((unsigned char)b[i + 1])) return 0;   /* octal */
            i = skip_ppnumber(b, i, len);
            continue;
        }
        if (strchr(" \t()+-*/%<>|&^~!={},", ch) == NULL) return 0;
        i++;
    }
    return 1;
}

int macro_passthrough_ok(const Macro *m) {
    if (m->is_builtin || m->is_variadic) return 0;
    if (g_c_mode && !c_passthrough_ok(m)) return 0;
    const char *b = m->body;
    size_t len = strlen(b), nlen = strlen(m->name);
    for (size_t i = 0; i < len; ) {
        if (b[i] == '"' || b[i] == '\'') { i = skip_literal(b, i, len); continue; }
        if (b[i] == '#') return 0;
        if (macro_is_ident_start((unsigned char)b[i])) {
            size_t j = i;
            while (j < len && macro_is_ident_char((unsigned char)b[j])) j++;
            if (!m->is_function && j - i == nlen && strncmp(b + i, m->name, nlen) == 0) return 0;
            i = j;
            continue;
        }
        if (isdigit((unsigned char)b[i])) { i = skip_ppnumber(b, i, len); continue; }
        i++;
    }
    return 1;
}

char *macro_format_define(const Macro *m) {
    Buf b;
    buf_init_from(&b, "#define ");
    buf_puts(&b, m->name);
    if (m->is_function) {
        buf_putc(&b, '(');
        for (int i = 0; i < m->param_count; i++) {
            if (i) buf_puts(&b, ", ");
            buf_puts(&b, m->params[i]);
        }
        if (m->is_variadic) buf_puts(&b, m->param_count ? ", ..." : "...");
        buf_putc(&b, ')');
    }
    if (m->body[0]) {
        /* one space for each whitespace run outside literals: a body
         * continued over several lines would otherwise keep every
         * line's indentation */
        buf_putc(&b, ' ');
        const char *p = m->body;
        size_t len = strlen(p);
        for (size_t i = 0; i < len; ) {
            if (p[i] == '"' || p[i] == '\'') {
                size_t e = skip_literal(p, i, len);
                buf_putn(&b, p + i, e - i);
                i = e;
            } else if (isspace((unsigned char)p[i])) {
                while (i < len && isspace((unsigned char)p[i])) i++;
                buf_putc(&b, ' ');
            } else {
                buf_putc(&b, p[i++]);
            }
        }
    }
    return b.s;
}

/* ---- expansion ------------------------------------------------------------ */

/* Is `m` a named constant whose uses can keep their name in the generated
 * C? Object-like, passed through to the C, and a body that is exactly one
 * numeric or character literal (`5`, `0x40`, `0.05`, `-1`, `'A'`). Such a use is
 * spliced as `@NAME@5`: lexer.l reads the annotation and the literal it
 * precedes carries NAME into the AST (AstNode.macro_name). */
static int is_named_constant(const Macro *m) {
    if (m->is_function || m->is_builtin || !macro_passthrough_ok(m)) return 0;
    const char *b = m->body;
    if (b[0] == '-' && (isdigit((unsigned char)b[1]) || b[1] == '.'))
        b++;    /* `-1`: lexer.l's "@NAME@-" rule folds the sign in */
    size_t len = strlen(b), end;
    if (len == 0) return 0;
    if (isdigit((unsigned char)b[0]) || (b[0] == '.' && isdigit((unsigned char)b[1])))
        end = skip_ppnumber(b, 0, len);
    else if (b[0] == '\'')
        end = skip_literal(b, 0, len);
    else
        return 0;
    return end == len;
}

/* Index past an `@NAME@` annotation starting at s[i] (s[i] == '@'), or i
 * itself when what's there isn't one. */
static size_t skip_annotation(const char *s, size_t i, size_t len) {
    size_t j = i + 1;
    if (j >= len || !macro_is_ident_start((unsigned char)s[j])) return i;
    while (j < len && macro_is_ident_char((unsigned char)s[j])) j++;
    return (j < len && s[j] == '@') ? j + 1 : i;
}

typedef struct Hide {
    const char *name;
    size_t end;          /* SIZE_MAX: hidden for the whole buffer */
} Hide;

typedef struct HideList {
    Hide *items;
    int count;
    int cap;
} HideList;

static void hide_push(HideList *h, const char *name, size_t end) {
    if (h->count == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 8;
        h->items = realloc(h->items, sizeof(Hide) * (size_t)h->cap);
    }
    h->items[h->count].name = name;
    h->items[h->count].end = end;
    h->count++;
}

static int is_hidden(const HideList *h, const char *name, size_t len, size_t pos) {
    for (int i = 0; i < h->count; i++)
        if (h->items[i].end > pos && strlen(h->items[i].name) == len &&
            strncmp(h->items[i].name, name, len) == 0)
            return 1;
    return 0;
}

/* Hides that cover `pos`, for passing into a nested (argument) expansion
 * where they then apply to the whole argument. */
static void hides_live_at(const HideList *h, size_t pos, HideList *out) {
    for (int i = 0; i < h->count; i++)
        if (h->items[i].end > pos) hide_push(out, h->items[i].name, SIZE_MAX);
}

static int expand_buffer(Buf *buf, int in_comment, int may_need_more,
                         const char *file, int line, HideList *hides, int depth);

/* Fully expands one macro argument on its own, as the standard requires
 * before substitution (except next to # or ##). */
static char *expand_arg(const char *raw, const HideList *live, const char *file, int line,
                        int depth, int *err) {
    Buf b;
    buf_init_from(&b, raw);
    HideList h = {0};
    for (int i = 0; i < live->count; i++) hide_push(&h, live->items[i].name, SIZE_MAX);
    if (expand_buffer(&b, 0, 0, file, line, &h, depth + 1) == MACRO_ERROR) *err = 1;
    free(h.items);
    char *r = dup_trimmed(b.s, 0, b.len);
    free(b.s);
    return r;
}

static char *stringize(const char *raw) {
    Buf b;
    buf_init_from(&b, "\"");
    size_t len = strlen(raw), i = 0;
    while (i < len && isspace((unsigned char)raw[i])) i++;
    while (len > i && isspace((unsigned char)raw[len - 1])) len--;
    int pending_space = 0;
    while (i < len) {
        if (isspace((unsigned char)raw[i])) { pending_space = 1; i++; continue; }
        if (raw[i] == '@' && skip_annotation(raw, i, len) > i) {
            i = skip_annotation(raw, i, len);
            continue;
        }
        if (pending_space) { buf_putc(&b, ' '); pending_space = 0; }
        if (raw[i] == '"' || raw[i] == '\'') {
            size_t e = skip_literal(raw, i, len);
            for (size_t k = i; k < e; k++) {
                if (raw[k] == '"' || raw[k] == '\\') buf_putc(&b, '\\');
                buf_putc(&b, raw[k]);
            }
            i = e;
            continue;
        }
        buf_putc(&b, raw[i++]);
    }
    buf_putc(&b, '"');
    return b.s;
}

/* Body tokens, just fine-grained enough for parameter substitution. */
typedef enum { TK_SPACE, TK_IDENT, TK_HASH, TK_PASTE, TK_OTHER } TokKind;
typedef struct { TokKind kind; size_t start, end; } BodyTok;

static BodyTok *tokenize_body(const char *b, int *count) {
    size_t len = strlen(b), i = 0;
    int cap = 16, n = 0;
    BodyTok *t = malloc(sizeof(BodyTok) * (size_t)cap);
    while (i < len) {
        if (n == cap) { cap *= 2; t = realloc(t, sizeof(BodyTok) * (size_t)cap); }
        size_t s = i;
        TokKind k;
        if (isspace((unsigned char)b[i])) { while (i < len && isspace((unsigned char)b[i])) i++; k = TK_SPACE; }
        else if (macro_is_ident_start((unsigned char)b[i])) { while (i < len && macro_is_ident_char((unsigned char)b[i])) i++; k = TK_IDENT; }
        else if (isdigit((unsigned char)b[i]) || (b[i] == '.' && isdigit((unsigned char)b[i + 1]))) { i = skip_ppnumber(b, i, len); k = TK_OTHER; }
        else if (b[i] == '"' || b[i] == '\'') { i = skip_literal(b, i, len); k = TK_OTHER; }
        else if (b[i] == '#' && b[i + 1] == '#') { i += 2; k = TK_PASTE; }
        else if (b[i] == '#') { i++; k = TK_HASH; }
        else { i++; k = TK_OTHER; }
        t[n].kind = k; t[n].start = s; t[n].end = i;
        n++;
    }
    *count = n;
    return t;
}

static int next_nonspace(const BodyTok *t, int n, int i) {
    for (i++; i < n; i++) if (t[i].kind != TK_SPACE) return i;
    return -1;
}

/* Appends `text`, separated from what precedes it by a space when the two
 * would otherwise lex as one token (unless `glue` -- a ## paste). */
static void append_piece(Buf *out, const char *text, size_t n, int glue) {
    if (n == 0) return;
    if (!glue && out->len > 0 && would_glue(out->s[out->len - 1], text[0])) buf_putc(out, ' ');
    buf_putn(out, text, n);
}

/* Builds the replacement for one function-like invocation. */
static char *substitute(const Macro *m, char **raw, int nraw, const HideList *live,
                        const char *file, int line, int depth, int *err) {
    int nparams = m->param_count + (m->is_variadic ? 1 : 0);
    char **expanded = calloc((size_t)(nparams ? nparams : 1), sizeof(char *));
    int ntok;
    BodyTok *t = tokenize_body(m->body, &ntok);
    const char *b = m->body;
    Buf out;
    buf_init_from(&out, "");
    int glue = 0;            /* previous token was ## */
    int after_subst = 0;     /* previous piece was substituted argument text */

    for (int i = 0; i < ntok; i++) {
        BodyTok tk = t[i];
        if (tk.kind == TK_SPACE) {
            if (!glue && out.len > 0) buf_putc(&out, ' ');
            continue;
        }
        if (tk.kind == TK_PASTE) {
            buf_rtrim(&out);
            glue = 1;
            continue;
        }
        if (tk.kind == TK_HASH && m->is_function) {
            int j = next_nonspace(t, ntok, i);
            int pi = param_index(m, b + t[j].start, t[j].end - t[j].start);
            char *s = stringize(pi < nraw ? raw[pi] : "");
            append_piece(&out, s, strlen(s), glue);
            free(s);
            i = j;
            glue = 0;
            after_subst = 0;
            continue;
        }
        int pi = (tk.kind == TK_IDENT && m->is_function)
                 ? param_index(m, b + tk.start, tk.end - tk.start) : -1;
        if (pi >= 0) {
            int nx = next_nonspace(t, ntok, i);
            int paste_next = (nx >= 0 && t[nx].kind == TK_PASTE);
            const char *rawtext = pi < nraw ? raw[pi] : "";
            if (glue || paste_next) {
                char *r = dup_trimmed(rawtext, 0, strlen(rawtext));
                if (glue && r[0] == '\0' && pi == m->param_count && m->is_variadic &&
                    out.len > 0 && out.s[out.len - 1] == ',') {
                    /* GNU `, ## __VA_ARGS__`: an empty variadic list
                     * swallows the comma before it. */
                    out.s[--out.len] = '\0';
                } else {
                    append_piece(&out, r, strlen(r), glue);
                }
                free(r);
            } else {
                if (expanded[pi] == NULL)
                    expanded[pi] = expand_arg(rawtext, live, file, line, depth, err);
                append_piece(&out, expanded[pi], strlen(expanded[pi]), 0);
            }
            glue = 0;
            after_subst = 1;
            continue;
        }
        /* ordinary body token: only needs separating from substituted text */
        if (after_subst && !glue)
            append_piece(&out, b + tk.start, tk.end - tk.start, 0);
        else
            buf_putn(&out, b + tk.start, tk.end - tk.start);
        glue = 0;
        after_subst = 0;
    }

    for (int i = 0; i < nparams; i++) free(expanded[i]);
    free(expanded);
    free(t);
    char *r = dup_trimmed(out.s, 0, out.len);
    free(out.s);
    return r;
}

/* From buf->s[open] == '(' collects the comma-separated arguments of an
 * invocation. Returns MACRO_OK (with *close = index past ')'),
 * MACRO_NEED_MORE when the buffer ends first. Arguments are raw text,
 * comments blanked; args and nargs receive malloc'd copies. */
static int collect_args(const Buf *buf, size_t open, int variadic_after, char ***args,
                        int *nargs, size_t *close) {
    const char *s = buf->s;
    size_t len = buf->len, i = open + 1, start = i;
    int depth = 0, n = 0, cap = 4;
    char **a = malloc(sizeof(char *) * (size_t)cap);
    Buf cur;
    buf_init_from(&cur, "");
    while (i < len) {
        char c = s[i];
        if (c == '"' || c == '\'') {
            size_t e = skip_literal(s, i, len);
            buf_putn(&cur, s + i, e - i);
            i = e;
            continue;
        }
        if (c == '/' && i + 1 < len && (s[i + 1] == '/' || s[i + 1] == '*')) {
            size_t e = skip_space_and_comments(s, i, len);
            if (e == len && s[i + 1] == '*' && strstr(s + i + 2, "*/") == NULL) break;
            buf_putc(&cur, ' ');
            i = e;
            continue;
        }
        if (c == '(') depth++;
        if (c == ')' && depth-- == 0) {
            if (n == cap) { cap *= 2; a = realloc(a, sizeof(char *) * (size_t)cap); }
            a[n++] = cur.s;
            *args = a;
            *nargs = n;
            *close = i + 1;
            return MACRO_OK;
        }
        if (c == ',' && depth == 0 && !(variadic_after >= 0 && n >= variadic_after)) {
            if (n == cap) { cap *= 2; a = realloc(a, sizeof(char *) * (size_t)cap); }
            a[n++] = cur.s;
            buf_init_from(&cur, "");
            i++;
            continue;
        }
        buf_putc(&cur, c == '\n' ? ' ' : c);
        i++;
    }
    (void)start;
    for (int k = 0; k < n; k++) free(a[k]);
    free(a);
    free(cur.s);
    return MACRO_NEED_MORE;
}

static void free_args(char **a, int n) {
    for (int i = 0; i < n; i++) free(a[i]);
    free(a);
}

/* Splices `repl` over [start, end), padding with spaces where the
 * replacement would otherwise fuse with its neighbors, and keeps every
 * hide region consistent. Returns the replacement's final length. */
static size_t splice_replacement(Buf *buf, size_t start, size_t end, const char *repl,
                                 HideList *hides) {
    char before = start > 0 ? buf->s[start - 1] : '\0';
    char after = end < buf->len ? buf->s[end] : '\0';
    size_t rlen = strlen(repl);
    Buf r;
    buf_init_from(&r, "");
    if (rlen == 0) {
        if (would_glue(before, after)) buf_putc(&r, ' ');
    } else {
        if (would_glue(before, repl[0])) buf_putc(&r, ' ');
        buf_puts(&r, repl);
        if (would_glue(repl[rlen - 1], after)) buf_putc(&r, ' ');
    }
    size_t old_len = end - start;
    for (int i = 0; i < hides->count; i++) {
        Hide *h = &hides->items[i];
        if (h->end == SIZE_MAX || h->end <= start) continue;
        if (h->end >= end) h->end = h->end - old_len + r.len;
        else h->end = start; /* invocation began inside, ended outside */
    }
    buf_splice(buf, start, end, r.s, r.len);
    size_t n = r.len;
    free(r.s);
    return n;
}

static int expand_buffer(Buf *buf, int in_comment, int may_need_more,
                         const char *file, int line, HideList *hides, int depth) {
    size_t pos = 0;
    long splices = 0;
    if (depth > V32CXX_MAX_MACRO_DEPTH) {
        fprintf(stderr, "%s:%d: error: macro expansion nested too deeply (more than %d levels)\n",
                file, line, V32CXX_MAX_MACRO_DEPTH);
        return MACRO_ERROR;
    }
    while (pos < buf->len) {
        const char *s = buf->s;
        size_t len = buf->len;
        char c = s[pos];
        if (in_comment) {
            if (c == '*' && pos + 1 < len && s[pos + 1] == '/') { in_comment = 0; pos += 2; }
            else pos++;
            continue;
        }
        if (c == '/' && pos + 1 < len && s[pos + 1] == '/') {
            while (pos < len && s[pos] != '\n') pos++;
            continue;
        }
        if (c == '/' && pos + 1 < len && s[pos + 1] == '*') { in_comment = 1; pos += 2; continue; }
        if (c == '"' || c == '\'') { pos = skip_literal(s, pos, len); continue; }
        if (isdigit((unsigned char)c) || (c == '.' && pos + 1 < len && isdigit((unsigned char)s[pos + 1]))) {
            pos = skip_ppnumber(s, pos, len);
            continue;
        }
        if (c == '@') {
            size_t after = skip_annotation(s, pos, len);
            pos = (after > pos) ? after : pos + 1;
            continue;
        }
        if (!macro_is_ident_start((unsigned char)c)) { pos++; continue; }

        size_t start = pos, end = pos;
        while (end < len && macro_is_ident_char((unsigned char)s[end])) end++;
        size_t nlen = end - start;
        char name[256];
        if (nlen >= sizeof(name)) { pos = end; continue; }
        memcpy(name, s + start, nlen);
        name[nlen] = '\0';

        Macro *m = table_find(name, NULL);
        if (m == NULL) {
            /* __FILE__ / __LINE__ (unless the program defined its own) */
            char val[600];
            if (strcmp(name, "__LINE__") == 0) snprintf(val, sizeof val, "%d", line);
            else if (strcmp(name, "__FILE__") == 0) snprintf(val, sizeof val, "\"%s\"", file);
            else { pos = end; continue; }
            pos = start + splice_replacement(buf, start, end, val, hides);
            continue;
        }
        if (is_hidden(hides, name, nlen, start)) { pos = end; continue; }

        if (++splices > 100000) {
            fprintf(stderr, "%s:%d: error: runaway macro expansion (while expanding '%s')\n",
                    file, line, name);
            return MACRO_ERROR;
        }

        if (!m->is_function) {
            char *body;
            if (is_named_constant(m)) {
                body = malloc(strlen(m->name) + strlen(m->body) + 3);
                sprintf(body, "@%s@%s", m->name, m->body);
            } else {
                body = strdup(m->body);
            }
            size_t n = splice_replacement(buf, start, end, body, hides);
            free(body);
            hide_push(hides, m->name, start + n);
            pos = start;
            continue;
        }

        /* function-like: an invocation only if '(' comes next */
        size_t p = skip_space_and_comments(s, end, len);
        if (p >= len) {
            if (may_need_more) return MACRO_NEED_MORE;
            pos = end;
            continue;
        }
        if (s[p] != '(') { pos = end; continue; }

        char **args = NULL;
        int nargs = 0;
        size_t close = 0;
        int variadic_after = m->is_variadic ? m->param_count : -1;
        if (collect_args(buf, p, variadic_after, &args, &nargs, &close) == MACRO_NEED_MORE) {
            if (may_need_more) return MACRO_NEED_MORE;
            fprintf(stderr, "%s:%d: error: unterminated argument list invoking macro '%s'\n",
                    file, line, name);
            return MACRO_ERROR;
        }

        /* `F()` is one empty argument: zero arguments for a zero-parameter
         * macro, one empty argument for a one-parameter macro. */
        int empty_call = (nargs == 1 && strspn(args[0], " \t\r\n") == strlen(args[0]));
        int ok;
        if (m->is_variadic) {
            /* collect_args stops splitting after the named parameters, so
             * nargs is at most param_count + 1; exactly param_count means
             * an empty __VA_ARGS__ (accepted, as C++20 does). */
            ok = (nargs >= m->param_count);
        } else if (m->param_count == 0) {
            ok = empty_call;
        } else {
            ok = (nargs == m->param_count);
        }
        if (!ok) {
            fprintf(stderr, "%s:%d: error: macro '%s' expects %d argument%s, but %d given\n",
                    file, line, name, m->param_count, m->param_count == 1 ? "" : "s",
                    empty_call ? 0 : nargs);
            free_args(args, nargs);
            return MACRO_ERROR;
        }
        if (m->param_count == 0 && !m->is_variadic) { free_args(args, nargs); args = NULL; nargs = 0; }

        HideList live = {0};
        hides_live_at(hides, start, &live);
        int err = 0;
        char *repl = substitute(m, args, nargs, &live, file, line, depth, &err);
        free(live.items);
        free_args(args, nargs);
        if (err) { free(repl); return MACRO_ERROR; }
        size_t n = splice_replacement(buf, start, close, repl, hides);
        free(repl);
        hide_push(hides, m->name, start + n);
        pos = start;
    }
    return MACRO_OK;
}

int macro_expand_text(const char *text, int starts_in_comment, int may_need_more,
                      const char *file, int line, char **out) {
    ensure_builtins();
    Buf b;
    buf_init_from(&b, text);
    HideList h = {0};
    int rc = expand_buffer(&b, starts_in_comment, may_need_more, file, line, &h, 0);
    free(h.items);
    if (rc != MACRO_OK) {
        free(b.s);
        *out = NULL;
        return rc;
    }
    *out = b.s;
    return MACRO_OK;
}

/* ---- #if expression evaluation ----------------------------------------- */

typedef struct Eval {
    const char *s;
    size_t pos;
    const char *file;
    int line;
    int error;
} Eval;

static void ev_space(Eval *e) {
    for (;;) {
        while (e->s[e->pos] && isspace((unsigned char)e->s[e->pos])) e->pos++;
        if (e->s[e->pos] != '@') return;
        size_t after = skip_annotation(e->s, e->pos, strlen(e->s));
        if (after == e->pos) return;
        e->pos = after;
    }
}

static int ev_accept(Eval *e, const char *op) {
    ev_space(e);
    size_t n = strlen(op);
    if (strncmp(e->s + e->pos, op, n) != 0) return 0;
    /* don't take '<' out of "<<" / "<=", '&' out of "&&", etc. */
    char next = e->s[e->pos + n];
    if (n == 1 && strchr("<>&|=!", op[0]) && (next == op[0] || next == '=')) {
        if (!(op[0] == '!' && next == op[0])) return 0;
    }
    if (n == 2 && (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0) && next == '=') return 0;
    e->pos += n;
    return 1;
}

static void ev_fail(Eval *e, const char *msg) {
    if (!e->error)
        fprintf(stderr, "%s:%d: error: %s in #if expression\n", e->file, e->line, msg);
    e->error = 1;
}

static long ev_cond(Eval *e, int live);

static long ev_primary(Eval *e, int live) {
    ev_space(e);
    const char *s = e->s;
    char c = s[e->pos];
    if (c == '(') {
        e->pos++;
        long v = ev_cond(e, live);
        if (!ev_accept(e, ")")) ev_fail(e, "missing ')'");
        return v;
    }
    if (isdigit((unsigned char)c)) {
        long v;
        char *endp;
        if (c == '0' && (s[e->pos + 1] == 'b' || s[e->pos + 1] == 'B'))
            v = strtol(s + e->pos + 2, &endp, 2);
        else
            v = strtol(s + e->pos, &endp, 0);
        e->pos = (size_t)(endp - s);
        while (strchr("uUlL", s[e->pos]) && s[e->pos]) e->pos++;
        if (s[e->pos] == '.' || macro_is_ident_char((unsigned char)s[e->pos]))
            ev_fail(e, "invalid integer constant");
        return v;
    }
    if (c == '\'') {
        long v = 0;
        size_t i = e->pos + 1;
        if (s[i] == '\\') {
            char x = s[i + 1];
            v = x == 'n' ? '\n' : x == 't' ? '\t' : x == 'r' ? '\r' : x == '0' ? 0 : x;
            i += 2;
        } else if (s[i]) {
            v = (unsigned char)s[i++];
        }
        if (s[i] != '\'') ev_fail(e, "malformed character constant");
        e->pos = i + 1;
        return v;
    }
    if (macro_is_ident_start((unsigned char)c)) {
        size_t st = e->pos;
        while (macro_is_ident_char((unsigned char)s[e->pos])) e->pos++;
        /* After expansion, any identifier left is not a macro: 0 -- except
         * C++'s own true/false. */
        if (e->pos - st == 4 && strncmp(s + st, "true", 4) == 0) return 1;
        return 0;
    }
    ev_fail(e, c ? "unexpected token" : "missing operand");
    return 0;
}

static long ev_unary(Eval *e, int live) {
    if (ev_accept(e, "!")) return !ev_unary(e, live);
    if (ev_accept(e, "~")) return ~ev_unary(e, live);
    if (ev_accept(e, "-")) return -ev_unary(e, live);
    if (ev_accept(e, "+")) return ev_unary(e, live);
    return ev_primary(e, live);
}

static long ev_mul(Eval *e, int live) {
    long v = ev_unary(e, live);
    for (;;) {
        if (ev_accept(e, "*")) v *= ev_unary(e, live);
        else if (ev_accept(e, "/")) {
            long r = ev_unary(e, live);
            if (r == 0) { if (live) ev_fail(e, "division by zero"); v = 0; }
            else v /= r;
        } else if (ev_accept(e, "%")) {
            long r = ev_unary(e, live);
            if (r == 0) { if (live) ev_fail(e, "division by zero"); v = 0; }
            else v %= r;
        } else return v;
    }
}

static long ev_add(Eval *e, int live) {
    long v = ev_mul(e, live);
    for (;;) {
        if (ev_accept(e, "+")) v += ev_mul(e, live);
        else if (ev_accept(e, "-")) v -= ev_mul(e, live);
        else return v;
    }
}

static long ev_shift(Eval *e, int live) {
    long v = ev_add(e, live);
    for (;;) {
        if (ev_accept(e, "<<")) v <<= ev_add(e, live);
        else if (ev_accept(e, ">>")) v >>= ev_add(e, live);
        else return v;
    }
}

static long ev_rel(Eval *e, int live) {
    long v = ev_shift(e, live);
    for (;;) {
        if (ev_accept(e, "<=")) v = v <= ev_shift(e, live);
        else if (ev_accept(e, ">=")) v = v >= ev_shift(e, live);
        else if (ev_accept(e, "<")) v = v < ev_shift(e, live);
        else if (ev_accept(e, ">")) v = v > ev_shift(e, live);
        else return v;
    }
}

static long ev_eq(Eval *e, int live) {
    long v = ev_rel(e, live);
    for (;;) {
        if (ev_accept(e, "==")) v = v == ev_rel(e, live);
        else if (ev_accept(e, "!=")) v = v != ev_rel(e, live);
        else return v;
    }
}

static long ev_band(Eval *e, int live) {
    long v = ev_eq(e, live);
    while (ev_accept(e, "&")) v &= ev_eq(e, live);
    return v;
}

static long ev_bxor(Eval *e, int live) {
    long v = ev_band(e, live);
    while (ev_accept(e, "^")) v ^= ev_band(e, live);
    return v;
}

static long ev_bor(Eval *e, int live) {
    long v = ev_bxor(e, live);
    while (ev_accept(e, "|")) v |= ev_bxor(e, live);
    return v;
}

static long ev_land(Eval *e, int live) {
    long v = ev_bor(e, live);
    while (ev_accept(e, "&&")) {
        long r = ev_bor(e, live && v);
        v = v && r;
    }
    return v;
}

static long ev_lor(Eval *e, int live) {
    long v = ev_land(e, live);
    while (ev_accept(e, "||")) {
        long r = ev_land(e, live && !v);
        v = v || r;
    }
    return v;
}

static long ev_cond(Eval *e, int live) {
    long c = ev_lor(e, live);
    if (ev_accept(e, "?")) {
        long a = ev_cond(e, live && c);
        if (!ev_accept(e, ":")) ev_fail(e, "expected ':'");
        long b = ev_cond(e, live && !c);
        return c ? a : b;
    }
    return c;
}

/* Replaces `defined NAME` / `defined ( NAME )` with 1 or 0 -- before
 * expansion, as the standard requires, so the operand is never expanded. */
static char *resolve_defined(const char *expr, const char *file, int line, int *err) {
    Buf b;
    buf_init_from(&b, "");
    size_t len = strlen(expr), i = 0;
    while (i < len) {
        if (expr[i] == '"' || expr[i] == '\'') {
            size_t e = skip_literal(expr, i, len);
            buf_putn(&b, expr + i, e - i);
            i = e;
            continue;
        }
        if (isdigit((unsigned char)expr[i])) {
            size_t e = skip_ppnumber(expr, i, len);
            buf_putn(&b, expr + i, e - i);
            i = e;
            continue;
        }
        if (macro_is_ident_start((unsigned char)expr[i])) {
            size_t st = i;
            while (i < len && macro_is_ident_char((unsigned char)expr[i])) i++;
            if (i - st == 7 && strncmp(expr + st, "defined", 7) == 0) {
                size_t j = i;
                while (j < len && isspace((unsigned char)expr[j])) j++;
                int paren = (j < len && expr[j] == '(');
                if (paren) { j++; while (j < len && isspace((unsigned char)expr[j])) j++; }
                size_t ns = j;
                while (j < len && macro_is_ident_char((unsigned char)expr[j])) j++;
                if (j == ns || !macro_is_ident_start((unsigned char)expr[ns])) {
                    fprintf(stderr, "%s:%d: error: operator \"defined\" requires an identifier\n", file, line);
                    *err = 1;
                    free(b.s);
                    return NULL;
                }
                char *name = dup_range(expr, ns, j);
                if (paren) {
                    while (j < len && isspace((unsigned char)expr[j])) j++;
                    if (j >= len || expr[j] != ')') {
                        fprintf(stderr, "%s:%d: error: missing ')' after \"defined\"\n", file, line);
                        *err = 1;
                        free(name);
                        free(b.s);
                        return NULL;
                    }
                    j++;
                }
                int is_def = macro_lookup(name) != NULL ||
                             strcmp(name, "__FILE__") == 0 || strcmp(name, "__LINE__") == 0;
                free(name);
                buf_puts(&b, is_def ? " 1 " : " 0 ");
                i = j;
                continue;
            }
            buf_putn(&b, expr + st, i - st);
            continue;
        }
        buf_putc(&b, expr[i++]);
    }
    return b.s;
}

int macro_eval_condition(const char *expr, const char *file, int line, long *value) {
    int err = 0;
    char *stripped = strip_comments(expr);
    if (stripped[0] == '\0') {
        fprintf(stderr, "%s:%d: error: #if with no expression\n", file, line);
        free(stripped);
        return -1;
    }
    char *resolved = resolve_defined(stripped, file, line, &err);
    free(stripped);
    if (err) return -1;
    char *expanded = NULL;
    if (macro_expand_text(resolved, 0, 0, file, line, &expanded) != MACRO_OK) {
        free(resolved);
        return -1;
    }
    free(resolved);
    Eval e = { expanded, 0, file, line, 0 };
    long v = ev_cond(&e, 1);
    ev_space(&e);
    if (!e.error && e.s[e.pos] != '\0') ev_fail(&e, "unexpected token");
    free(expanded);
    if (e.error) return -1;
    *value = v;
    return 0;
}
