#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "sema.h"

static int g_error_count = 0;

static void sema_error(int line, const char *fmt, ...) {
    fprintf(stderr, "semantic error at line %d: ", line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    g_error_count++;
}

/* ---- flat class registry, keyed by bare (unqualified) name -----------
 *
 * SIMPLIFICATION: keyed by bare name only, ignoring namespace nesting.
 * Fine for this skeleton's test programs (class names don't collide
 * across namespaces yet), but means a namespaced class's out-of-line
 * definitions resolve against *any* same-named class anywhere, not
 * specifically the one in the right namespace. Flagged again at the
 * out_of_line_def grammar rules in parser.y and in attach_out_of_line()
 * below, at the point it actually matters.
 */

typedef struct ClassRegEntry {
    const char *name;
    AstNode *decl;
    struct ClassRegEntry *next;
} ClassRegEntry;

static ClassRegEntry *g_class_registry = NULL;

static void register_class(AstNode *class_decl) {
    ClassRegEntry *e = malloc(sizeof(ClassRegEntry));
    e->name = class_decl->str1;
    e->decl = class_decl;
    e->next = g_class_registry;
    g_class_registry = e;
}

static AstNode *find_class(const char *name) {
    for (ClassRegEntry *e = g_class_registry; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) return e->decl;
    }
    return NULL;
}

static void free_registry(void) {
    ClassRegEntry *e = g_class_registry;
    while (e != NULL) {
        ClassRegEntry *next = e->next;
        free(e);
        e = next;
    }
    g_class_registry = NULL;
}

/* ---- pass 1: collect every class, recursing into namespace bodies ---- */

static void collect_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            register_class(n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            collect_classes(&n->list);
        }
    }
}

/* ---- type-signature comparison and mangling ---------------------------
 *
 * PURELY SYNTACTIC, not semantic: two type AST nodes are compared by their
 * written shape (same kind, same names, same pointer/reference nesting),
 * NOT by resolving typedefs to their underlying type. That means:
 *
 *   typedef int MyInt;
 *   void f(int x);
 *   void f(MyInt x);
 *
 * ...is treated as two DIFFERENT signatures here, when real C++ would
 * consider it an invalid redeclaration (MyInt IS int). Fixing that needs
 * typedef resolution -- walking the symbol table to find what a TYPE_NAME
 * naming a typedef actually resolves to, recursively (a typedef can name
 * another typedef) -- which needs access to the SymTab that parsing built,
 * not just the AST sema.c currently walks. Worth doing before this is
 * trusted for anything beyond straightforward, typedef-free overloads;
 * flagged rather than silently wrong.
 */

static int types_equal(const AstNode *t1, const AstNode *t2) {
    if (t1 == NULL || t2 == NULL) {
        return t1 == t2;
    }
    if (t1->kind != t2->kind) {
        return 0;
    }
    switch (t1->kind) {
        case AST_IDENT:
            return strcmp(t1->str1, t2->str1) == 0;
        case AST_QUALIFIED_ID: {
            if (t1->list.count != t2->list.count) return 0;
            for (int i = 0; i < t1->list.count; i++) {
                if (strcmp(t1->list.items[i]->str1, t2->list.items[i]->str1) != 0) {
                    return 0;
                }
            }
            return 1;
        }
        case AST_POINTER_TYPE:
        case AST_REFERENCE_TYPE:
            return types_equal(t1->a, t2->a);
        default:
            /* Not a type-position AST kind -- shouldn't happen given what
             * the parser puts in a `type` slot, but fail closed (treat as
             * "not equal") rather than crash or silently match. */
            return 0;
    }
}

static int param_lists_match(const AstList *a, const AstList *b) {
    if (a->count != b->count) {
        return 0;
    }
    for (int i = 0; i < a->count; i++) {
        if (!types_equal(a->items[i]->type, b->items[i]->type)) {
            return 0;
        }
    }
    return 1;
}

/* Renders a type as a short, C-identifier-safe fragment for use inside a
 * mangled name -- e.g. Timer -> "Timer", `Timer *` -> "Timer_ptr",
 * `v32::Timer &` -> "v32_Timer_ref". "::" and "*"/"&" aren't legal in a C
 * identifier, hence the "_"-joining and "_ptr"/"_ref" suffixes instead of
 * just splicing the written syntax in verbatim. */
static char *type_signature_str(const AstNode *type) {
    if (type == NULL) {
        return strdup("void");
    }
    switch (type->kind) {
        case AST_IDENT:
            return strdup(type->str1);
        case AST_QUALIFIED_ID: {
            size_t len = 1;
            for (int i = 0; i < type->list.count; i++) {
                len += strlen(type->list.items[i]->str1) + 1;
            }
            char *out = malloc(len);
            out[0] = '\0';
            for (int i = 0; i < type->list.count; i++) {
                if (i > 0) strcat(out, "_");
                strcat(out, type->list.items[i]->str1);
            }
            return out;
        }
        case AST_POINTER_TYPE:
        case AST_REFERENCE_TYPE: {
            char *inner = type_signature_str(type->a);
            const char *suffix = (type->kind == AST_POINTER_TYPE) ? "ptr" : "ref";
            size_t len = strlen(inner) + strlen(suffix) + 2;
            char *out = malloc(len);
            snprintf(out, len, "%s_%s", inner, suffix);
            free(inner);
            return out;
        }
        default:
            return strdup("unknown");
    }
}

/* Joins every parameter's type_signature_str with "_", or "void" for an
 * empty parameter list -- matching the C convention of writing `f(void)`
 * for "takes nothing," which reads better in a mangled name than a bare
 * trailing "__". */
static char *param_signature_str(const AstList *params) {
    if (params->count == 0) {
        return strdup("void");
    }
    char *acc = strdup("");
    for (int i = 0; i < params->count; i++) {
        char *part = type_signature_str(params->items[i]->type);
        size_t len = strlen(acc) + strlen(part) + 2;
        char *joined = malloc(len);
        if (i == 0) {
            snprintf(joined, len, "%s", part);
        } else {
            snprintf(joined, len, "%s_%s", acc, part);
        }
        free(acc);
        free(part);
        acc = joined;
    }
    return acc;
}

/*
 * "Class__method__paramSig" for a member, "method__paramSig" for a free
 * function. Destructors get special-cased: the AST stores a destructor's
 * name as e.g. "~Counter" (see func_header in parser.y), and '~' is not a
 * legal C identifier character -- splicing it in unmodified would have
 * produced a mangled name codegen could never actually emit. "dtor" is
 * used in its place; the class name already disambiguates which class's
 * destructor it is, so the (redundant) original class name isn't repeated
 * inside the dtor's own name component.
 */
static char *mangle(const char *class_name, const char *method_name, const AstList *params) {
    const char *name_part = method_name;
    if (method_name[0] == '~') {
        name_part = "dtor";
    }

    char *param_sig = param_signature_str(params);
    char *out;
    if (class_name == NULL) {
        size_t len = strlen(name_part) + 2 + strlen(param_sig) + 1;
        out = malloc(len);
        snprintf(out, len, "%s__%s", name_part, param_sig);
    } else {
        size_t len = strlen(class_name) + 2 + strlen(name_part) + 2 + strlen(param_sig) + 1;
        out = malloc(len);
        snprintf(out, len, "%s__%s__%s", class_name, name_part, param_sig);
    }
    free(param_sig);
    return out;
}

static FuncSemaInfo *make_func_info(const char *class_name, const char *method_name,
                                     const AstList *params, int is_out_of_line) {
    FuncSemaInfo *info = calloc(1, sizeof(FuncSemaInfo));
    info->mangled_name = mangle(class_name, method_name, params);
    info->is_out_of_line = is_out_of_line;
    return info;
}

/* ---- pass 2: attach out-of-line definitions onto their prototype ------ */

static void attach_out_of_line(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];

        if (n->kind == AST_NAMESPACE_DECL) {
            attach_out_of_line(&n->list);
            continue;
        }
        if (n->kind != AST_FUNC_DEF || n->b == NULL) {
            continue; /* an ordinary function/method def, not out-of-line */
        }

        /* n->b is an AST_QUALIFIED_ID: the qualifier chain the definition
         * was written against (e.g. [Player] for `Player::update`, or
         * [v32, Timer] for `v32::Timer::method`). We resolve using only
         * the LAST component's name -- see the registry's doc comment
         * above for why that's a real gap for namespaced classes. */
        AstList *qual_parts = &n->b->list;
        const char *class_name = qual_parts->items[qual_parts->count - 1]->str1;

        AstNode *class_decl = find_class(class_name);
        if (class_decl == NULL) {
            sema_error(n->line, "out-of-line definition of '%s' names unknown class '%s'",
                       n->str1, class_name);
            continue;
        }

        AstNode *target = NULL;
        for (int j = 0; j < class_decl->list.count; j++) {
            AstNode *member = class_decl->list.items[j];
            if (member->kind == AST_FUNC_DECL &&
                member->str1 != NULL && strcmp(member->str1, n->str1) == 0 &&
                param_lists_match(&member->list, &n->list)) {
                /* Full name-AND-signature match: this is what makes
                 * `Vector::Vector(int)` attach to the right constructor
                 * when Vector() and Vector(int) both exist, instead of
                 * both out-of-line ctor definitions colliding onto
                 * whichever same-named prototype happened to come first
                 * in the class body. */
                target = member;
                break;
            }
        }

        if (target == NULL) {
            sema_error(n->line, "no matching declaration for out-of-line definition of '%s' in class '%s'",
                       n->str1, class_name);
            continue;
        }

        target->kind = AST_FUNC_DEF;
        target->a = n->a;
        target->sema_info = make_func_info(class_name, n->str1, &target->list, 0);

        /* Leave the top-level duplicate in the AST (codegen needs
         * *something* stable to skip over rather than silently vanishing
         * nodes mid-pass) but flag it so codegen knows not to re-emit it
         * -- the authoritative copy is now reachable via the class's
         * member list / ClassLayout.methods. */
        n->sema_info = make_func_info(class_name, n->str1, &n->list, 1);
    }
}

/* ---- pass 3: per-class layout ----------------------------------------- */

/* ---- vtable slot assignment --------------------------------------------
 *
 * Single inheritance only (matches this project's whole scope), which is
 * what keeps this tractable: a derived class has exactly one base, so
 * "this class's vtable" is unambiguously "the base's vtable, with some
 * slots overridden and maybe some new ones appended" -- no diamond
 * inheritance, no virtual-base-class slot-sharing puzzles to solve.
 */

/* Two virtual methods occupy the SAME slot if they have the same name and
 * parameter signature -- EXCEPT destructors, which all share one
 * conceptual slot per class hierarchy regardless of their (necessarily
 * class-specific) literal spelling ("~Base" vs "~Derived" are different
 * strings but the same override relationship in real C++). */
static const char *vtable_slot_key(const AstNode *method) {
    return (method->str1[0] == '~') ? "~" : method->str1;
}

static int vtable_find_slot(const Vtable *vt, const AstNode *candidate) {
    const char *key = vtable_slot_key(candidate);
    for (int i = 0; i < vt->count; i++) {
        AstNode *existing = vt->entries[i].method;
        if (strcmp(vtable_slot_key(existing), key) == 0 &&
            param_lists_match(&existing->list, &candidate->list)) {
            return i;
        }
    }
    return -1;
}

static void vtable_append_slot(Vtable *vt, AstNode *method) {
    if (vt->count == vt->capacity) {
        vt->capacity = vt->capacity ? vt->capacity * 2 : 4;
        vt->entries = realloc(vt->entries, sizeof(VtableEntry) * (size_t)vt->capacity);
    }
    vt->entries[vt->count].method = method;
    vt->entries[vt->count].slot_index = vt->count;
    vt->count++;
}

/* Builds `layout->vtable` for the class `layout` belongs to. Must be
 * called only after layout->base_class_decl (if any) has ALREADY had its
 * own layout -- and therefore its own vtable -- computed; see the call
 * site in compute_layout() for how that's guaranteed regardless of
 * top-to-bottom visitation order. */
static void build_vtable(ClassLayout *layout) {
    Vtable *base_vtable = NULL;
    if (layout->base_class_decl != NULL) {
        ClassLayout *base_layout = (ClassLayout *)layout->base_class_decl->sema_info;
        base_vtable = (base_layout != NULL) ? base_layout->vtable : NULL;
    }

    Vtable *vt = calloc(1, sizeof(Vtable));

    /* Inherit every slot from the base's vtable first, pointing at
     * whichever AstNode currently implements it there -- overwritten
     * below wherever this class actually overrides it. */
    if (base_vtable != NULL) {
        for (int i = 0; i < base_vtable->count; i++) {
            vtable_append_slot(vt, base_vtable->entries[i].method);
        }
    }

    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        int slot = vtable_find_slot(vt, m);
        if (slot >= 0) {
            /* Overriding an inherited slot. Real C++ treats this as
             * virtual even if 'virtual' isn't repeated on the override --
             * match that here rather than requiring the keyword again at
             * every level of the hierarchy. */
            vt->entries[slot].method = m;
            m->ival = 1;
        } else if (m->ival == 1) {
            /* A genuinely new virtual method (or a virtual destructor
             * introduced at this level, if the base had none). */
            vtable_append_slot(vt, m);
        }
        /* else: an ordinary, non-virtual, non-overriding method -- not
         * part of any vtable at all. */
    }

    if (vt->count == 0) {
        free(vt);
        layout->vtable = NULL;
    } else {
        layout->vtable = vt;
    }
}

static void compute_layout(AstNode *class_decl) {
    if (class_decl->sema_info != NULL) {
        return; /* already computed -- this happens when a class is
                  * visited here as another class's base before
                  * compute_layouts()'s own top-level loop reaches it
                  * directly; see the recursive call below. */
    }

    ClassLayout *layout = calloc(1, sizeof(ClassLayout));
    layout->data_members = ast_list_new();
    layout->methods = ast_list_new();

    for (int i = 0; i < class_decl->list.count; i++) {
        AstNode *member = class_decl->list.items[i];
        if (member->kind == AST_VAR_DECL) {
            ast_list_append(&layout->data_members, member);
        } else if (member->kind == AST_FUNC_DECL || member->kind == AST_FUNC_DEF) {
            ast_list_append(&layout->methods, member);
            if (member->sema_info == NULL) {
                /* Wasn't touched by attach_out_of_line (either it's an
                 * in-class-only method, or it never got a body at all --
                 * still fine to mangle a bodyless prototype). */
                member->sema_info = make_func_info(class_decl->str1, member->str1, &member->list, 0);
            }
        }
        /* AST_ACCESS_SPEC markers are intentionally not represented in
         * either list -- access control enforcement (is this member
         * reachable from here?) isn't implemented yet, see the README. */
    }

    if (class_decl->str2 != NULL) {
        layout->base_class_decl = find_class(class_decl->str2);
        if (layout->base_class_decl == NULL) {
            sema_error(class_decl->line, "class '%s' inherits from unknown base '%s'",
                       class_decl->str1, class_decl->str2);
        } else {
            /* Ensure the base's layout (and vtable) exists BEFORE this
             * class's own vtable is built, regardless of which order
             * compute_layouts()'s top-level walk happens to visit
             * classes in. Can't cycle: the parser requires a base class
             * to already be a registered TYPE_NAME before it can be
             * named in `opt_base`, so a class can never (even
             * transitively) end up inheriting from itself. */
            compute_layout(layout->base_class_decl);
        }
        /* Deliberately not merging the base's DATA members into
         * data_members/methods here -- see the ClassLayout doc comment
         * in sema.h. Virtual METHODS are handled differently, by
         * build_vtable() below, since a vtable specifically needs
         * inherited slots carried forward. */
    }

    build_vtable(layout);

    class_decl->sema_info = layout;
}

static void compute_layouts(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            compute_layout(n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            compute_layouts(&n->list);
        }
    }
}

/* ---- pass 4: mangle free functions (anything not already handled by a
 * class's layout pass above) --------------------------------------------- */

static void mangle_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            mangle_free_functions(&n->list);
        } else if ((n->kind == AST_FUNC_DECL || n->kind == AST_FUNC_DEF) && n->sema_info == NULL) {
            n->sema_info = make_func_info(NULL, n->str1, &n->list, 0);
        }
    }
}

int sema_run(AstNode *program) {
    g_error_count = 0;
    free_registry(); /* defensive: in case sema_run() is ever called twice in one process */

    collect_classes(&program->list);
    attach_out_of_line(&program->list);
    compute_layouts(&program->list);
    mangle_free_functions(&program->list);

    free_registry();
    return g_error_count;
}

/* ---- dump ---------------------------------------------------------- */

static void indent_line(int indent) {
    for (int i = 0; i < indent; i++) fputs("  ", stdout);
}

static void dump_class_layout(const AstNode *class_decl, int indent) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;

    indent_line(indent);
    printf("class %s", class_decl->str1);
    if (class_decl->str2 != NULL) printf(" : %s", class_decl->str2);
    printf("\n");

    if (layout == NULL) {
        indent_line(indent + 1);
        printf("(no layout computed)\n");
        return;
    }

    indent_line(indent + 1);
    printf("data members:\n");
    for (int i = 0; i < layout->data_members.count; i++) {
        indent_line(indent + 2);
        printf("%s\n", layout->data_members.items[i]->str1);
    }

    indent_line(indent + 1);
    printf("methods:\n");
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        FuncSemaInfo *info = (FuncSemaInfo *)m->sema_info;
        indent_line(indent + 2);
        printf("%s -> %s%s%s\n",
               m->str1,
               info != NULL ? info->mangled_name : "(unmangled)",
               m->kind == AST_FUNC_DEF ? " [has body]" : " [prototype only]",
               m->ival == 1 ? " [virtual]" : "");
    }

    if (layout->base_class_decl != NULL) {
        indent_line(indent + 1);
        printf("base class resolved: yes (%s)\n", layout->base_class_decl->str1);
    } else if (class_decl->str2 != NULL) {
        indent_line(indent + 1);
        printf("base class resolved: NO (dangling reference to '%s')\n", class_decl->str2);
    }

    indent_line(indent + 1);
    if (layout->vtable == NULL) {
        printf("vtable: (none -- no virtual methods, own or inherited)\n");
    } else {
        printf("vtable:\n");
        for (int i = 0; i < layout->vtable->count; i++) {
            AstNode *m = layout->vtable->entries[i].method;
            FuncSemaInfo *info = (FuncSemaInfo *)m->sema_info;
            indent_line(indent + 2);
            printf("[%d] %s -> %s\n",
                   layout->vtable->entries[i].slot_index,
                   m->str1,
                   info != NULL ? info->mangled_name : "(unmangled)");
        }
    }
}

static void dump_decls(const AstList *decls, int indent) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            dump_class_layout(n, indent);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            indent_line(indent);
            printf("namespace %s {\n", n->str1);
            dump_decls(&n->list, indent + 1);
            indent_line(indent);
            printf("}\n");
        } else if (n->kind == AST_FUNC_DECL || n->kind == AST_FUNC_DEF) {
            FuncSemaInfo *info = (FuncSemaInfo *)n->sema_info;
            if (info != NULL && info->is_out_of_line) {
                /* Skip the top-level duplicate left behind by an
                 * out-of-line definition -- its real entry already
                 * printed under the owning class above. */
                continue;
            }
            indent_line(indent);
            printf("function %s -> %s%s\n",
                   n->str1,
                   info != NULL ? info->mangled_name : "(unmangled)",
                   n->kind == AST_FUNC_DEF ? " [has body]" : " [prototype only]");
        }
    }
}

void sema_dump(const AstNode *program) {
    printf("---- semantic analysis summary ----\n");
    dump_decls(&program->list, 0);
}
