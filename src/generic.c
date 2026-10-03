/*
 * generic.c -- transpiler-level `std::array<T, N>`. See generic.h for
 * the overview.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generic.h"
#include "driver.h"
#include "prescan.h"

int g_generic_array_enabled = 0;
int g_using_std = 0;

extern FILE *yyin;
int yyparse(void);
void yyrestart(FILE *f);

/*
 * The class every std::array<T, N> becomes. @NAME@, @T@ and @N@ are
 * replaced textually; the result is ordinary v32c++ input.
 *
 * Differences from the real std::array, all forced by the target:
 *   - sizes and indices are `int` (Vircon32 has one integer type);
 *   - iterators are plain `T *`;
 *   - at() does not throw (no exceptions here) -- it is operator[]
 *     under another name;
 *   - size() is written with sizeof rather than as `return @N@;` so the
 *     generated C function uses its `this` argument: the Vircon32 C
 *     compiler warns about every unused argument, and three such
 *     warnings per instantiation soon hit its warning limit. The value
 *     is still a compile-time constant;
 *   - m_data is public, so `std::array<int, 3> a = {1, 2, 3};` stays
 *     plain aggregate initialization.
 */
static const char *const ARRAY_TEMPLATE =
    "class @NAME@ {\n"
    "public:\n"
    "    @T@ m_data[@N@];\n"
    "    int size() const { return sizeof(m_data) / sizeof(m_data[0]); }\n"
    "    int max_size() const { return size(); }\n"
    "    bool empty() const { return size() == 0; }\n"
    "    @T@ &operator[](int i) { return m_data[i]; }\n"
    "    @T@ &at(int i) { return m_data[i]; }\n"
    "    @T@ &front() { return m_data[0]; }\n"
    "    @T@ &back() { return m_data[@N@ - 1]; }\n"
    "    @T@ *data() { return m_data; }\n"
    "    @T@ *begin() { return m_data; }\n"
    "    @T@ *end() { return m_data + @N@; }\n"
    "    void fill(@T@ value) {\n"
    "        for (int i = 0; i < @N@; i++) m_data[i] = value;\n"
    "    }\n"
    "};\n";

typedef struct Instance {
    char *name;      /* array_Enemy_8 */
    char *elem_text; /* Enemy */
    char *len_text;  /* 8 */
    int emitted;     /* already parsed and spliced */
    const char *file; /* where it was first named, for diagnostics */
    int line;
    struct Instance *next;
} Instance;

static Instance *g_instances = NULL, *g_instances_tail = NULL;

/* ---- small string builder ------------------------------------------ */

typedef struct { char *s; size_t len, cap; } Str;

static void str_add(Str *b, const char *t) {
    size_t n = strlen(t);
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2;
        b->s = realloc(b->s, b->cap);
    }
    memcpy(b->s + b->len, t, n + 1);
    b->len += n;
}

/* Renders an element type back to source text (`text`) and to the piece
 * of the class name it contributes (`mangle`). Returns 0 for a type
 * that cannot be an array element. */
static int render_type(const AstNode *t, Str *text, Str *mangle) {
    if (t == NULL) return 0;
    switch (t->kind) {
        case AST_IDENT:
            str_add(text, t->str1);
            str_add(mangle, t->str1);
            return 1;
        case AST_QUALIFIED_ID:
            for (int i = 0; i < t->list.count; i++) {
                if (i > 0) { str_add(text, "::"); str_add(mangle, "_"); }
                str_add(text, t->list.items[i]->str1);
                str_add(mangle, t->list.items[i]->str1);
            }
            return 1;
        case AST_CONST_TYPE:
            str_add(text, "const ");
            str_add(mangle, "const_");
            return render_type(t->a, text, mangle);
        case AST_POINTER_TYPE:
            if (!render_type(t->a, text, mangle)) return 0;
            str_add(text, " *");
            str_add(mangle, "_ptr");
            return 1;
        default:
            return 0; /* references, arrays, function pointers */
    }
}

AstNode *generic_array_type(AstNode *elem, int len_value, const char *len_name, int line) {
    if (!g_generic_array_enabled) {
        fprintf(stderr, "%s:%d: error: std::array needs #include <array>\n",
                g_current_filename, line);
        g_parse_errors++;
        return NULL;
    }
    Str text = {0}, mangle = {0};
    str_add(&mangle, "array_");
    if (!render_type(elem, &text, &mangle) || strcmp(text.s, "void") == 0) {
        fprintf(stderr, "%s:%d: error: std::array element type must be a plain type "
                "or a pointer (not void, a reference, an array or a function pointer)\n",
                g_current_filename, line);
        g_parse_errors++;
        free(text.s); free(mangle.s);
        return NULL;
    }
    if (len_name == NULL && len_value <= 0) {
        fprintf(stderr, "%s:%d: error: std::array length must be greater than zero\n",
                g_current_filename, line);
        g_parse_errors++;
        free(text.s); free(mangle.s);
        return NULL;
    }
    char len_text[64];
    if (len_name != NULL) snprintf(len_text, sizeof len_text, "%s", len_name);
    else                  snprintf(len_text, sizeof len_text, "%d", len_value);
    str_add(&mangle, "_");
    str_add(&mangle, len_text);

    Instance *inst;
    for (inst = g_instances; inst != NULL; inst = inst->next) {
        if (strcmp(inst->name, mangle.s) == 0) break;
    }
    if (inst == NULL) {
        inst = calloc(1, sizeof *inst);
        inst->name = strdup(mangle.s);
        inst->elem_text = strdup(text.s);
        inst->len_text = strdup(len_text);
        inst->file = g_current_filename;
        inst->line = line;
        if (g_instances_tail != NULL) g_instances_tail->next = inst;
        else                          g_instances = inst;
        g_instances_tail = inst;
    }
    AstNode *type = ast_ident(inst->name, line);
    free(text.s); free(mangle.s);
    return type;
}

/* ---- instantiation -------------------------------------------------- */

static void write_instance(FILE *out, const Instance *inst) {
    /* A pointer element type goes through a typedef: this grammar has no
     * `T *&` or `T **` declarators, which `@T@ &operator[]` and
     * `@T@ *begin()` would otherwise need. */
    char *elem = strdup(inst->elem_text);
    if (strchr(inst->elem_text, '*') != NULL) {
        free(elem);
        elem = malloc(strlen(inst->name) + 6);
        sprintf(elem, "%s_elem", inst->name);
        fprintf(out, "typedef %s%s;\n", inst->elem_text, elem);
    }
    for (const char *p = ARRAY_TEMPLATE; *p != '\0'; ) {
        if      (strncmp(p, "@NAME@", 6) == 0) { fputs(inst->name, out);      p += 6; }
        else if (strncmp(p, "@T@", 3) == 0)    { fputs(elem, out);            p += 3; }
        else if (strncmp(p, "@N@", 3) == 0)    { fputs(inst->len_text, out);  p += 3; }
        else                                   { fputc(*p, out);              p++;    }
    }
    free(elem);
}

/* Does the subtree under `n` name the type `name` anywhere? */
static int mentions(const AstNode *n, const char *name) {
    if (n == NULL) return 0;
    if (n->kind == AST_IDENT && n->str1 != NULL && strcmp(n->str1, name) == 0) return 1;
    if (mentions(n->type, name) || mentions(n->a, name) || mentions(n->b, name) ||
        mentions(n->c, name) || mentions(n->d, name)) return 1;
    for (int i = 0; i < n->list.count; i++) {
        if (mentions(n->list.items[i], name)) return 1;
    }
    return 0;
}

static void list_insert(AstList *list, int at, AstNode *node) {
    ast_list_append(list, node); /* grows the list by one */
    for (int i = list->count - 1; i > at; i--) list->items[i] = list->items[i - 1];
    list->items[at] = node;
}

/* Finds the class called `name` at any namespace depth. */
static const AstNode *find_class(const AstList *decls, const char *name) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_CLASS_DECL && n->str1 != NULL && strcmp(n->str1, name) == 0) return n;
        if (n->kind == AST_NAMESPACE_DECL) {
            const AstNode *found = find_class(&n->list, name);
            if (found != NULL) return found;
        }
    }
    return NULL;
}

/* The elements live in a member array, and this project does not yet
 * run constructors or destructors for class-typed member ARRAYS (see
 * phase 9a's note in lower.c: "no member arrays, either side"). Say so
 * rather than leave the elements silently unconstructed. */
static void warn_if_elements_need_construction(const AstNode *program, const Instance *inst) {
    const char *last = strrchr(inst->elem_text, ':');
    const AstNode *c = find_class(&program->list, last != NULL ? last + 1 : inst->elem_text);
    if (c == NULL) return;
    const char *what = NULL;
    for (int i = 0; i < c->list.count && what == NULL; i++) {
        const AstNode *m = c->list.items[i];
        if (m == NULL || (m->kind != AST_FUNC_DECL && m->kind != AST_FUNC_DEF) || m->str1 == NULL) continue;
        if (strcmp(m->str1, c->str1) == 0) what = "constructor";
        else if (m->str1[0] == '~')        what = "destructor";
        else if (m->ival)                  what = "virtual functions";
    }
    if (what == NULL) return;
    fprintf(stderr,
        "%s:%d: warning: std::array<%s, %s>: %s has a %s, but the elements of a\n"
        "      std::array are not constructed or destroyed yet (they start out uninitialized,\n"
        "      like the fields of a plain struct). Initialize each element by hand, or hold\n"
        "      pointers instead: std::array<%s *, %s>.\n",
        inst->file, inst->line, inst->elem_text, inst->len_text, c->str1,
        strcmp(what, "virtual functions") == 0 ? "vtable (virtual functions)" : what,
        inst->elem_text, inst->len_text);
}

static int is_instance_name(const char *name) {
    for (Instance *i = g_instances; i != NULL; i = i->next) {
        if (strcmp(i->name, name) == 0) return 1;
    }
    return 0;
}

/* `std::array<int, 3> a = {1, 2, 3};` initializes the class's one
 * member, the inner array, with the braces around it left out. C++
 * and standard C allow that; Vircon32 C does not ("too many values to
 * assign to structure"), so the inner braces are put back here:
 * `{{1, 2, 3}}`. */
static void wrap_array_initializers(AstNode *n) {
    if (n == NULL) return;
    if (n->kind == AST_VAR_DECL && n->a != NULL && n->a->kind == AST_INIT_LIST) {
        const AstNode *t = n->type;
        while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
        int already_nested = n->a->list.count == 1 && n->a->list.items[0]->kind == AST_INIT_LIST;
        if (t != NULL && t->kind == AST_IDENT && t->str1 != NULL &&
            is_instance_name(t->str1) && !already_nested) {
            AstNode *outer = ast_new(AST_INIT_LIST, n->a->line);
            ast_list_append(&outer->list, n->a);
            n->a = outer;
        }
    }
    wrap_array_initializers(n->a);
    wrap_array_initializers(n->b);
    wrap_array_initializers(n->c);
    wrap_array_initializers(n->d);
    for (int i = 0; i < n->list.count; i++) wrap_array_initializers(n->list.items[i]);
}

int generic_instantiate_pending(void) {
    AstNode *program = g_program;
    const char *saved_file = g_current_filename;
    int saved_line = g_lex_lineno;
    int rc = 0;

    for (;;) {
        /* Write out every instance not yet emitted. Parsing them can
         * queue more (a generic class that itself uses another), hence
         * the outer loop. */
        Instance *first = NULL;
        for (Instance *i = g_instances; i != NULL; i = i->next) {
            if (!i->emitted) { first = i; break; }
        }
        if (first == NULL) break;

        FILE *f = tmpfile();
        if (f == NULL) { perror("tmpfile"); rc = 1; break; }
        fputs("# 1 \"<std::array>\"\n", f);
        Instance *last = NULL;
        for (Instance *i = first; i != NULL; i = i->next) {
            if (!i->emitted) { write_instance(f, i); last = i; }
        }
        rewind(f);

        FILE *saved_in = yyin;
        yyin = f;
        yyrestart(f);
        g_program = NULL;
        int prc = yyparse();
        fclose(f);
        yyin = saved_in;
        if (prc != 0 || g_parse_errors > 0 || g_program == NULL) {
            fprintf(stderr, "---- internal error: a generated std::array class failed to parse ----\n");
            rc = 1;
            break;
        }

        AstNode *generated = g_program;
        for (Instance *i = first; i != NULL; i = i->next) {
            if (i->emitted) { if (i == last) break; continue; }
            /* This instance's declarations: everything in the generated
             * program up to and including its class (a typedef for a
             * pointer element type comes first). */
            int end = -1;
            for (int k = 0; k < generated->list.count; k++) {
                AstNode *d = generated->list.items[k];
                if (d != NULL && d->kind == AST_CLASS_DECL && d->str1 != NULL && strcmp(d->str1, i->name) == 0) {
                    end = k;
                    break;
                }
            }
            if (end >= 0) {
                if (strchr(i->elem_text, '*') == NULL) warn_if_elements_need_construction(program, i);
                int start = end;
                while (start > 0 && generated->list.items[start - 1] != NULL &&
                       generated->list.items[start - 1]->kind != AST_CLASS_DECL) start--;
                int at = program->list.count;
                for (int k = 0; k < program->list.count; k++) {
                    if (mentions(program->list.items[k], i->name)) { at = k; break; }
                }
                for (int k = start; k <= end; k++) {
                    list_insert(&program->list, at++, generated->list.items[k]);
                }
            }
            i->emitted = 1;
            if (i == last) break;
        }
    }

    if (rc == 0) wrap_array_initializers(program);
    g_program = program;
    g_current_filename = saved_file;
    g_lex_lineno = saved_line;
    return rc;
}
