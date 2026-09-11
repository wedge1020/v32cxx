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

/* ---- mangling --------------------------------------------------------
 *
 * First cut: "Class__method" for members, bare name for free functions.
 * KNOWN GAP: no parameter-type encoding, so overloaded methods/functions
 * (same name, different params) currently mangle to the SAME string --
 * that will collide the moment codegen tries to emit two C functions with
 * identical names. Needed before codegen can handle overloading: extend
 * this to fold in a type-based suffix once params carry resolved,
 * comparable type information (which itself needs the pointer/reference
 * wrapping already in the AST to be paired with resolved class/typedef
 * identity, not just raw ast_ident/QualifiedId text).
 */

static char *mangle(const char *class_name, const char *method_name) {
    if (class_name == NULL) {
        return strdup(method_name);
    }
    size_t len = strlen(class_name) + 2 + strlen(method_name) + 1;
    char *out = malloc(len);
    snprintf(out, len, "%s__%s", class_name, method_name);
    return out;
}

static FuncSemaInfo *make_func_info(const char *class_name, const char *method_name, int is_out_of_line) {
    FuncSemaInfo *info = calloc(1, sizeof(FuncSemaInfo));
    info->mangled_name = mangle(class_name, method_name);
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
                member->str1 != NULL && strcmp(member->str1, n->str1) == 0) {
                /* TODO: name-only match -- the first same-named prototype
                 * wins. Wrong the moment an overloaded method is defined
                 * out-of-line; needs real parameter-signature comparison,
                 * same gap noted on mangle() above. */
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
        target->sema_info = make_func_info(class_name, n->str1, 0);

        /* Leave the top-level duplicate in the AST (codegen needs
         * *something* stable to skip over rather than silently vanishing
         * nodes mid-pass) but flag it so codegen knows not to re-emit it
         * -- the authoritative copy is now reachable via the class's
         * member list / ClassLayout.methods. */
        n->sema_info = make_func_info(class_name, n->str1, 1);
    }
}

/* ---- pass 3: per-class layout ----------------------------------------- */

static void compute_layout(AstNode *class_decl) {
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
                member->sema_info = make_func_info(class_decl->str1, member->str1, 0);
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
        }
        /* Deliberately not merging the base's members into data_members/
         * methods here -- see the ClassLayout doc comment in sema.h. */
    }

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
            n->sema_info = make_func_info(NULL, n->str1, 0);
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
        printf("%s -> %s%s\n",
               m->str1,
               info != NULL ? info->mangled_name : "(unmangled)",
               m->kind == AST_FUNC_DEF ? " [has body]" : " [prototype only]");
    }

    if (layout->base_class_decl != NULL) {
        indent_line(indent + 1);
        printf("base class resolved: yes (%s)\n", layout->base_class_decl->str1);
    } else if (class_decl->str2 != NULL) {
        indent_line(indent + 1);
        printf("base class resolved: NO (dangling reference to '%s')\n", class_decl->str2);
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
