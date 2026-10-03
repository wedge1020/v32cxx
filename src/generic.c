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
int g_generic_vector_enabled = 0;
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
    "    void fill(const @T@ &value) {\n"
    "        for (int i = 0; i < @N@; i++) m_data[i] = value;\n"
    "    }\n"
    "};\n";

/*
 * The class every std::vector<T> becomes.
 *
 * Differences from the real std::vector:
 *   - sizes and indices are `int`; iterators are plain `T *`, so
 *     `v.erase(v.begin() + i)` and `v.insert(v.begin(), x)` read as usual;
 *   - elements are COPIED IN AND MOVED AROUND AS RAW MEMORY (struct
 *     assignment). The vector never runs an element's constructor or
 *     destructor: push_back copies the object it is given, resize(n)
 *     zero-fills, and pop_back/clear/erase just forget elements. That is
 *     exactly right for numbers, pointers and plain structs, and fine for
 *     a class whose constructor only fills in fields (virtual functions
 *     keep working: the vtable pointer is copied with the rest). A class
 *     with a DESTRUCTOR is the one that loses out, and gets a warning;
 *   - storage comes from Vircon32's malloc()/free(). There are no
 *     exceptions, so running out of memory is not reported;
 *   - `a = b` copies every element (operator= below). Other copies --
 *     `vector<T> b = a;`, passing or returning one by value -- would
 *     leave two vectors owning one buffer and are rejected: use a
 *     reference.
 *
 * resize() clears new elements through a `char *`, which is one word per
 * step on Vircon32 and one byte in standard C: `sizeof` counts in the
 * same unit either way.
 */
static const char *const VECTOR_TEMPLATE =
    "class @NAME@ {\n"
    "public:\n"
    "    @T@ *m_data;\n"
    "    int m_size;\n"
    "    int m_capacity;\n"
    "    @NAME@() { m_data = 0; m_size = 0; m_capacity = 0; }\n"
    "    ~@NAME@() { free(m_data); }\n"
    "    int size() const { return m_size; }\n"
    "    int capacity() const { return m_capacity; }\n"
    "    bool empty() const { return m_size == 0; }\n"
    "    @T@ &operator[](int i) { return m_data[i]; }\n"
    "    @T@ &at(int i) { return m_data[i]; }\n"
    "    @T@ &front() { return m_data[0]; }\n"
    "    @T@ &back() { return m_data[m_size - 1]; }\n"
    "    @T@ *data() { return m_data; }\n"
    "    @T@ *begin() { return m_data; }\n"
    "    @T@ *end() { return m_data + m_size; }\n"
    "    void reserve(int n) {\n"
    "        if (n <= m_capacity) return;\n"
    "        @T@ *grown = (@T@ *)malloc(n * sizeof(@T@));\n"
    "        for (int i = 0; i < m_size; i++) grown[i] = m_data[i];\n"
    "        free(m_data);\n"
    "        m_data = grown;\n"
    "        m_capacity = n;\n"
    "    }\n"
    "    void push_back(const @T@ &value) {\n"
    "        if (m_size == m_capacity) {\n"
    "            @T@ copy = value;\n"
    "            if (m_capacity == 0) reserve(4); else reserve(m_capacity * 2);\n"
    "            m_data[m_size] = copy;\n"
    "        } else {\n"
    "            m_data[m_size] = value;\n"
    "        }\n"
    "        m_size++;\n"
    "    }\n"
    "    void pop_back() { m_size--; }\n"
    "    void clear() { m_size = 0; }\n"
    "    void resize(int n) {\n"
    "        if (n > m_size) {\n"
    "            reserve(n);\n"
    "            char *raw = (char *)(m_data + m_size);\n"
    "            int count = (n - m_size) * sizeof(@T@);\n"
    "            for (int i = 0; i < count; i++) raw[i] = 0;\n"
    "        }\n"
    "        m_size = n;\n"
    "    }\n"
    "    void resize(int n, const @T@ &value) {\n"
    "        @T@ copy = value;\n"
    "        reserve(n);\n"
    "        for (int i = m_size; i < n; i++) m_data[i] = copy;\n"
    "        m_size = n;\n"
    "    }\n"
    "    @T@ *erase(@T@ *pos) {\n"
    "        @T@ *last = m_data + m_size - 1;\n"
    "        for (@T@ *p = pos; p < last; p++) p[0] = p[1];\n"
    "        m_size--;\n"
    "        return pos;\n"
    "    }\n"
    "    @T@ *insert(@T@ *pos, const @T@ &value) {\n"
    "        int index = pos - m_data;\n"
    "        @T@ copy = value;\n"
    "        if (m_size == m_capacity) {\n"
    "            if (m_capacity == 0) reserve(4); else reserve(m_capacity * 2);\n"
    "        }\n"
    "        for (int i = m_size; i > index; i--) m_data[i] = m_data[i - 1];\n"
    "        m_data[index] = copy;\n"
    "        m_size++;\n"
    "        return m_data + index;\n"
    "    }\n"
    "    @NAME@ &operator=(const @NAME@ &other) {\n"
    "        if (this != &other) {\n"
    "            m_size = 0;\n"
    "            reserve(other.m_size);\n"
    "            for (int i = 0; i < other.m_size; i++) m_data[i] = other.m_data[i];\n"
    "            m_size = other.m_size;\n"
    "        }\n"
    "        return *this;\n"
    "    }\n"
    "};\n";

typedef struct Instance {
    char *name;      /* array_Enemy_8 */
    char *elem_text; /* Enemy */
    char *len_text;  /* 8 */
    int emitted;     /* already parsed and spliced */
    int is_vector;
    AstNode *elem_type; /* the element type as parsed */
    char *ns_open;      /* "namespace si { " for each enclosing namespace, or "" */
    int ns_depth;
    int line;        /* where it was first named, for diagnostics */
    const char *file;
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

static AstNode *instance_type(int is_vector, AstNode *elem, const char *len_text, int line) {
    const char *what = is_vector ? "std::vector" : "std::array";
    if (is_vector ? !g_generic_vector_enabled : !g_generic_array_enabled) {
        fprintf(stderr, "%s:%d: error: %s needs #include <%s>\n",
                g_current_filename, line, what, is_vector ? "vector" : "array");
        g_parse_errors++;
        return NULL;
    }
    Str text = {0}, mangle = {0};
    str_add(&mangle, is_vector ? "vector_" : "array_");
    if (!render_type(elem, &text, &mangle) || strcmp(text.s, "void") == 0) {
        fprintf(stderr, "%s:%d: error: %s element type must be a plain type "
                "or a pointer (not void, a reference, an array or a function pointer)\n",
                g_current_filename, line, what);
        g_parse_errors++;
        free(text.s); free(mangle.s);
        return NULL;
    }
    if (len_text != NULL) {
        str_add(&mangle, "_");
        str_add(&mangle, len_text);
    }

    Instance *inst;
    for (inst = g_instances; inst != NULL; inst = inst->next) {
        if (strcmp(inst->name, mangle.s) == 0) break;
    }
    if (inst == NULL) {
        inst = calloc(1, sizeof *inst);
        inst->name = strdup(mangle.s);
        inst->elem_text = strdup(text.s);
        inst->len_text = strdup(len_text != NULL ? len_text : "");
        inst->is_vector = is_vector;
        inst->elem_type = elem;
        /* The namespaces the type was named in, outermost first: the
         * class has to be generated INSIDE them, or an element type
         * declared there (`Bullet` in `namespace si`) means nothing. */
        {
            const char *names[16];
            int depth = 0;
            for (Scope *sc = g_symtab->current; sc != NULL && depth < 16; sc = sc->parent) {
                if (sc->owner_name != NULL && !sc->is_class_scope) names[depth++] = sc->owner_name;
            }
            Str open = {0};
            str_add(&open, "");
            for (int k = depth - 1; k >= 0; k--) {
                str_add(&open, "namespace ");
                str_add(&open, names[k]);
                str_add(&open, " { ");
            }
            inst->ns_open = open.s;
            inst->ns_depth = depth;
        }
        inst->line = line;
        inst->file = g_current_filename;
        if (g_instances_tail != NULL) g_instances_tail->next = inst;
        else                          g_instances = inst;
        g_instances_tail = inst;
        /* the vector's malloc()/free() come from the same header new and
         * delete use */
        if (is_vector) g_uses_new_or_delete = 1;
    }
    AstNode *type = ast_ident(inst->name, line);
    free(text.s); free(mangle.s);
    return type;
}

AstNode *generic_array_type(AstNode *elem, int len_value, const char *len_name, int line) {
    if (len_name == NULL && len_value <= 0) {
        fprintf(stderr, "%s:%d: error: std::array length must be greater than zero\n",
                g_current_filename, line);
        g_parse_errors++;
        return NULL;
    }
    char len_text[64];
    if (len_name != NULL) snprintf(len_text, sizeof len_text, "%s", len_name);
    else                  snprintf(len_text, sizeof len_text, "%d", len_value);
    return instance_type(0, elem, len_text, line);
}

AstNode *generic_vector_type(AstNode *elem, int line) {
    return instance_type(1, elem, NULL, line);
}

/* ---- instantiation -------------------------------------------------- */

/* The generated text spells a POINTER element type as a typedef name,
 * `<class>_elem`, because the grammar has no `T *&` or `T **`
 * declarators (`@T@ &operator[]`, `@T@ *begin()`). The AST has no such
 * limit, so once the class is parsed every use of that name is replaced
 * by the real pointer type and the typedef itself is dropped: nothing
 * downstream ever sees it. */
static void replace_elem_typedef(AstNode *n, const char *alias, const AstNode *elem) {
    if (n == NULL) return;
    if (n->kind == AST_IDENT && n->str1 != NULL && strcmp(n->str1, alias) == 0) {
        int line = n->line;
        const char *file = n->file;
        *n = *elem;
        n->line = line;
        n->file = file;
        return;
    }
    replace_elem_typedef(n->type, alias, elem);
    replace_elem_typedef(n->a, alias, elem);
    replace_elem_typedef(n->b, alias, elem);
    replace_elem_typedef(n->c, alias, elem);
    replace_elem_typedef(n->d, alias, elem);
    for (int i = 0; i < n->list.count; i++) replace_elem_typedef(n->list.items[i], alias, elem);
}

static void write_instance(FILE *out, const Instance *inst) {
    /* See replace_elem_typedef, above. */
    char *elem = strdup(inst->elem_text);
    fprintf(out, "%s\n", inst->ns_open);
    if (strchr(inst->elem_text, '*') != NULL) {
        free(elem);
        elem = malloc(strlen(inst->name) + 6);
        sprintf(elem, "%s_elem", inst->name);
        fprintf(out, "typedef %s%s;\n", inst->elem_text, elem);
    }
    for (const char *p = inst->is_vector ? VECTOR_TEMPLATE : ARRAY_TEMPLATE; *p != '\0'; ) {
        if      (strncmp(p, "@NAME@", 6) == 0) { fputs(inst->name, out);      p += 6; }
        else if (strncmp(p, "@T@", 3) == 0)    { fputs(elem, out);            p += 3; }
        else if (strncmp(p, "@N@", 3) == 0)    { fputs(inst->len_text, out);  p += 3; }
        else                                   { fputc(*p, out);              p++;    }
    }
    for (int k = 0; k < inst->ns_depth; k++) fputs("}\n", out);
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

static const Instance *find_instance(const char *name) {
    for (Instance *i = g_instances; i != NULL; i = i->next) {
        if (strcmp(i->name, name) == 0) return i;
    }
    return NULL;
}

int g_no_inline_containers = 0;

int generic_class_kind(const char *class_name) {
    const Instance *i = (class_name != NULL) ? find_instance(class_name) : NULL;
    if (i == NULL) return 0;
    return i->is_vector ? 2 : 1;
}

static int is_instance_name(const char *name) {
    const Instance *i = find_instance(name);
    return i != NULL && !i->is_vector;
}

/* Is `type` (through any `const`) a std::vector, held BY VALUE? */
static int is_vector_value_type(const AstNode *type) {
    while (type != NULL && type->kind == AST_CONST_TYPE) type = type->a;
    if (type == NULL || type->kind != AST_IDENT || type->str1 == NULL) return 0;
    const Instance *i = find_instance(type->str1);
    return i != NULL && i->is_vector;
}

static void vector_error(const AstNode *n, const char *why) {
    fprintf(stderr, "%s:%d: error: %s\n", n->file != NULL ? n->file : "?", n->line, why);
    g_parse_errors++;
}

/* A std::vector owns a heap buffer, and this project has no copy
 * constructors: copying the struct would leave two vectors freeing one
 * buffer. `a = b` is safe (operator= copies the elements); every other
 * way of copying one is caught here, from the declarations alone. */
static void reject_vector_copies(const AstNode *n, int in_generated_class) {
    if (n == NULL) return;
    if (n->kind == AST_CLASS_DECL && n->str1 != NULL && find_instance(n->str1) != NULL) in_generated_class = 1;
    if (!in_generated_class) {
        if ((n->kind == AST_FUNC_DEF || n->kind == AST_FUNC_DECL)) {
            if (is_vector_value_type(n->type))
                vector_error(n, "a std::vector can't be returned by value: fill one passed in by reference");
            for (int i = 0; i < n->list.count; i++) {
                const AstNode *p = n->list.items[i];
                if (p != NULL && is_vector_value_type(p->type))
                    vector_error(p, "a std::vector can't be passed by value (it would share its "
                                    "storage with the original): pass a reference, `std::vector<T> &`");
            }
        }
        if (n->kind == AST_VAR_DECL && n->a != NULL && is_vector_value_type(n->type))
            vector_error(n, "a std::vector can't be initialized from another value: declare it "
                            "empty, then assign (`b = a;` copies the elements) or push_back");
    }
    reject_vector_copies(n->a, in_generated_class);
    reject_vector_copies(n->b, in_generated_class);
    reject_vector_copies(n->c, in_generated_class);
    reject_vector_copies(n->d, in_generated_class);
    for (int i = 0; i < n->list.count; i++) reject_vector_copies(n->list.items[i], in_generated_class);
}

/* The vector never runs an element's destructor (see VECTOR_TEMPLATE). */
static void warn_if_elements_have_destructor(const AstList *decls, const Instance *inst) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_NAMESPACE_DECL) { warn_if_elements_have_destructor(&n->list, inst); continue; }
        const char *last = strrchr(inst->elem_text, ':');
        if (n->kind != AST_CLASS_DECL || n->str1 == NULL ||
            strcmp(n->str1, last != NULL ? last + 1 : inst->elem_text) != 0) continue;
        for (int m = 0; m < n->list.count; m++) {
            const AstNode *f = n->list.items[m];
            if (f == NULL || (f->kind != AST_FUNC_DECL && f->kind != AST_FUNC_DEF) ||
                f->str1 == NULL || f->str1[0] != '~') continue;
            fprintf(stderr,
                "%s:%d: warning: std::vector<%s>: %s has a destructor, but a std::vector does not\n"
                "      run it for its elements (they are copied in and dropped as raw memory).\n"
                "      Whatever the destructor releases is yours to release, or hold pointers:\n"
                "      std::vector<%s *>.\n",
                inst->file, inst->line, inst->elem_text, n->str1, inst->elem_text);
            return;
        }
    }
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

/* The generated class, wherever the namespaces around it put it. */
static AstNode *find_generated_class(const AstList *decls, const char *name) {
    for (int k = 0; k < decls->count; k++) {
        AstNode *d = decls->items[k];
        if (d == NULL) continue;
        if (d->kind == AST_CLASS_DECL && d->str1 != NULL && strcmp(d->str1, name) == 0) return d;
        if (d->kind == AST_NAMESPACE_DECL) {
            AstNode *found = find_generated_class(&d->list, name);
            if (found != NULL) return found;
        }
    }
    return NULL;
}

/* Inserts `decl` just ahead of the first declaration that names it. When
 * that declaration is a namespace, the search continues inside it, so
 * the class lands next to its first use, in the same namespace. */
static void place_before_first_use(AstList *decls, const char *name, AstNode *decl) {
    for (int k = 0; k < decls->count; k++) {
        AstNode *d = decls->items[k];
        if (d == NULL || !mentions(d, name)) continue;
        if (d->kind == AST_NAMESPACE_DECL) place_before_first_use(&d->list, name, decl);
        else list_insert(decls, k, decl);
        return;
    }
    list_insert(decls, decls->count, decl);
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
        fputs("# 1 \"<generic>\"\n", f);
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
            fprintf(stderr, "---- internal error: a generated std::array/std::vector class failed to parse ----\n");
            rc = 1;
            break;
        }

        AstNode *generated = g_program;
        for (Instance *i = first; i != NULL; i = i->next) {
            if (i->emitted) { if (i == last) break; continue; }
            AstNode *decl = find_generated_class(&generated->list, i->name);
            if (decl != NULL) {
                if (i->is_vector && strchr(i->elem_text, '*') == NULL)
                    warn_if_elements_have_destructor(&program->list, i);
                if (strchr(i->elem_text, '*') != NULL) {
                    char alias[256];
                    snprintf(alias, sizeof alias, "%s_elem", i->name);
                    replace_elem_typedef(decl, alias, i->elem_type);
                }
                place_before_first_use(&program->list, i->name, decl);
            }
            i->emitted = 1;
            if (i == last) break;
        }
    }

    if (rc == 0) {
        wrap_array_initializers(program);
        int before = g_parse_errors;
        reject_vector_copies(program, 0);
        if (g_parse_errors > before) rc = 1;
    }
    g_program = program;
    g_current_filename = saved_file;
    g_lex_lineno = saved_line;
    return rc;
}
