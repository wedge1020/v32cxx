/* ****************************************************************************
 *  cmode.c -- C input: the AST passes that run between the parser and the
 *  semantic analyzer when the file being transpiled is C (g_c_mode).
 *
 *  v32c++ reads C as a subset of C++, and nearly all of it is. These passes
 *  cover the places where C says something C++ does not, rewriting each
 *  into a form the rest of the pipeline (written for C++) already handles:
 *
 *    cmode_separate_tags        struct/union/enum tags have their own
 *                               namespace in C; the output has one
 *    cmode_unify_prototypes     `void fatal();` then `void fatal(char *s)
 *                               { ... }` is one function, not two overloads
 *    cmode_lower_main_params    main(argc, argv, envp)
 *    cmode_rewrite_variadics    calls of `...` functions
 *
 *  See docs/C_INPUT.md for the whole picture, including what lower.c and
 *  codegen.c do for C input further down the pipeline.
 * ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "driver.h"
#include "cmode.h"

/* ---- a small set of names -------------------------------------------------- */

typedef struct NameSet {
    char **names;
    int *values;
    int count, cap;
} NameSet;

static int name_set_find(const NameSet *s, const char *name) {
    for (int i = 0; i < s->count; i++)
        if (strcmp(s->names[i], name) == 0) return i;
    return -1;
}

static int name_set_has(const NameSet *s, const char *name) {
    return name_set_find(s, name) >= 0;
}

static void name_set_add(NameSet *s, const char *name, int value) {
    int at = name_set_find(s, name);
    if (at >= 0) { s->values[at] = value; return; }
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 64;
        s->names = realloc(s->names, sizeof(char *) * (size_t)s->cap);
        s->values = realloc(s->values, sizeof(int) * (size_t)s->cap);
    }
    s->names[s->count] = strdup(name);
    s->values[s->count] = value;
    s->count++;
}

static void name_set_free(NameSet *s) {
    for (int i = 0; i < s->count; i++) free(s->names[i]);
    free(s->names);
    free(s->values);
}

static int is_function(const AstNode *n) {
    return n != NULL && (n->kind == AST_FUNC_DECL || n->kind == AST_FUNC_DEF) && n->str1 != NULL;
}

/* ---- tags -------------------------------------------------------------------
 *
 * C keeps struct, union and enum tags in a namespace of their own:
 *
 *     struct rdes { ... } rdes[MAXROOMS];     -- a type AND a variable
 *
 * Vircon32 C names a struct by its bare name, in the one namespace it has,
 * so a tag that is also the name of a variable, function, parameter,
 * typedef or enum constant is renamed (`rdes_tag`), in its definition and
 * in every `struct rdes` the parser saw (g_tag_refs). Struct members are
 * not ordinary names -- they cannot be confused with a type -- and are not
 * counted.
 */
static void collect_ordinary_names(const AstNode *n, NameSet *names);

static void collect_ordinary_names_list(const AstList *list, NameSet *names) {
    for (int i = 0; i < list->count; i++) collect_ordinary_names(list->items[i], names);
}

static void collect_ordinary_names(const AstNode *n, NameSet *names) {
    if (n == NULL) return;
    switch (n->kind) {
        case AST_CLASS_DECL:
        case AST_UNION_DECL:
            return;                             /* members: not ordinary names */
        case AST_VAR_DECL:
        case AST_PARAM:
        case AST_TYPEDEF_DECL:
        case AST_ENUM_VALUE:
        case AST_FUNC_DECL:
        case AST_FUNC_DEF:
            if (n->str1 != NULL) name_set_add(names, n->str1, 0);
            break;
        default:
            break;
    }
    collect_ordinary_names(n->a, names);
    collect_ordinary_names(n->b, names);
    collect_ordinary_names(n->c, names);
    collect_ordinary_names(n->d, names);
    collect_ordinary_names_list(&n->list, names);
}

void cmode_separate_tags(AstList *decls) {
    NameSet ordinary = { NULL, NULL, 0, 0 };
    NameSet tags = { NULL, NULL, 0, 0 };
    collect_ordinary_names_list(decls, &ordinary);
    for (int i = 0; i < g_tag_decls.count; i++)
        name_set_add(&tags, g_tag_decls.items[i]->str1, 0);

    for (int i = 0; i < g_tag_decls.count; i++) {
        AstNode *d = g_tag_decls.items[i];
        if (!name_set_has(&ordinary, d->str1)) continue;
        char *old_name = d->str1;
        size_t len = strlen(old_name);
        char *new_name = malloc(len + 16);
        sprintf(new_name, "%s_tag", old_name);
        for (int n = 2; name_set_has(&ordinary, new_name) || name_set_has(&tags, new_name); n++)
            sprintf(new_name, "%s_tag%d", old_name, n);
        name_set_add(&tags, new_name, 0);
        for (int r = 0; r < g_tag_refs.count; r++) {
            AstNode *ref = g_tag_refs.items[r];
            if (strcmp(ref->str1, old_name) == 0) ref->str1 = strdup(new_name);
        }
        for (int j = 0; j < g_tag_decls.count; j++) {
            AstNode *o = g_tag_decls.items[j];
            if (o != d && strcmp(o->str1, old_name) == 0) o->str1 = strdup(new_name);
        }
        d->str1 = new_name;
    }
    name_set_free(&ordinary);
    name_set_free(&tags);
}

/* ---- prototypes ---------------------------------------------------------------
 *
 * In C a function is one function however often it is declared, and a
 * declaration may say less than the definition does:
 *
 *     void fatal();               -- K&R: parameters not given
 *     void leave(int);            -- unnamed
 *     void fatal(char *s) { }     -- the definition
 *
 * To the C++ pipeline those would be overloads. So each function keeps its
 * definition and, when it was declared earlier, that first declaration --
 * rewritten to say exactly what the definition says. Later declarations
 * go, and so does a function that is declared and never defined: Vircon32
 * C builds one file with no linker, so there is nothing such a prototype
 * could ever refer to (the SDK's own functions are declared by the SDK
 * headers, which pass through untouched). A function declared inside
 * another function's body (`int count_strings();`) is treated as if it had
 * been declared at file scope.
 */
static void remove_local_prototypes(AstNode *n, AstList *found) {
    if (n == NULL) return;
    if (n->kind == AST_BLOCK || n->kind == AST_SWITCH) {
        AstList kept = ast_list_new();
        for (int i = 0; i < n->list.count; i++) {
            AstNode *s = n->list.items[i];
            if (s != NULL && s->kind == AST_FUNC_DECL) {
                ast_list_append(found, s);
                continue;
            }
            remove_local_prototypes(s, found);
            ast_list_append(&kept, s);
        }
        n->list = kept;
    }
    switch (n->kind) {
        case AST_IF:    remove_local_prototypes(n->b, found);
                        remove_local_prototypes(n->c, found); break;
        case AST_WHILE: remove_local_prototypes(n->b, found); break;
        case AST_FOR:   remove_local_prototypes(n->d, found); break;
        case AST_LABEL: remove_local_prototypes(n->a, found); break;
        default: break;
    }
}

/* A copy of a declaration's node that shares nothing later phases write
 * to: the node, its children and its types. */
static AstNode *clone_with_types(const AstNode *n) {
    if (n == NULL) return NULL;
    AstNode *c = malloc(sizeof *c);
    *c = *n;
    if (n->str1 != NULL) c->str1 = strdup(n->str1);
    if (n->str2 != NULL) c->str2 = strdup(n->str2);
    c->type = clone_with_types(n->type);
    c->a = clone_with_types(n->a);
    c->b = clone_with_types(n->b);
    c->c = clone_with_types(n->c);
    c->d = clone_with_types(n->d);
    c->list = ast_list_new();
    for (int i = 0; i < n->list.count; i++) ast_list_append(&c->list, clone_with_types(n->list.items[i]));
    return c;
}

void cmode_unify_prototypes(AstList *decls) {
    /* local prototypes out of the bodies, onto the end of the file */
    AstList locals = ast_list_new();
    for (int i = 0; i < decls->count; i++)
        if (decls->items[i]->kind == AST_FUNC_DEF) remove_local_prototypes(decls->items[i]->a, &locals);
    AstList all = ast_list_new();
    for (int i = 0; i < decls->count; i++) ast_list_append(&all, decls->items[i]);
    for (int i = 0; i < locals.count; i++) ast_list_append(&all, locals.items[i]);

    AstList out = ast_list_new();
    for (int i = 0; i < all.count; i++) {
        AstNode *n = all.items[i];
        if (!is_function(n)) {
            ast_list_append(&out, n);
            continue;
        }
        AstNode *first = NULL, *def = NULL;
        for (int j = 0; j < all.count; j++) {
            AstNode *o = all.items[j];
            if (!is_function(o) || strcmp(o->str1, n->str1) != 0) continue;
            if (first == NULL) first = o;
            if (def == NULL && o->kind == AST_FUNC_DEF) def = o;
        }
        if (def == NULL) continue;      /* declared, never defined: nothing to call */
        if (n == def) {
            ast_list_append(&out, n);
        } else if (n == first) {
            /* the first declaration stays where it is, saying what the
             * definition says */
            n->type = clone_with_types(def->type);
            n->list = ast_list_new();
            for (int p = 0; p < def->list.count; p++)
                ast_list_append(&n->list, clone_with_types(def->list.items[p]));
            ast_list_append(&out, n);
        }
    }
    *decls = out;
}

/* ---- main(argc, argv, envp) --------------------------------------------------
 *
 * A cartridge is not run from a command line: Vircon32's main takes
 * nothing. A C program's main keeps its parameters as locals holding what
 * a program run with no arguments would see:
 *
 *     void main(void) {
 *         int argc = 1;
 *         char *__v32_argv[2] = { "program", NULL };
 *         char **argv = __v32_argv;
 *         char *__v32_envp[1] = { NULL };
 *         char **envp = __v32_envp;
 */
static AstNode *main_string_array(const char *name, const char *first, int line) {
    AstNode *init = ast_new(AST_INIT_LIST, line);
    if (first != NULL) {
        AstNode *s = ast_new(AST_STRING_LIT, line);
        s->str1 = strdup(first);
        ast_list_append(&init->list, s);
    }
    ast_list_append(&init->list, ast_new(AST_NULL_LIT, line));
    AstNode *v = ast_new(AST_VAR_DECL, line);
    v->str1 = strdup(name);
    v->type = ast_wrap_array(ast_wrap_pointer(ast_ident("char", line), line), init->list.count, line);
    v->a = init;
    return v;
}

void cmode_lower_main_params(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *f = decls->items[i];
        if (f->kind != AST_FUNC_DEF || f->str1 == NULL || strcmp(f->str1, "main") != 0) continue;
        if (f->list.count == 0 || f->a == NULL || f->a->kind != AST_BLOCK) continue;
        int line = f->line;
        AstList body = ast_list_new();
        for (int p = 0; p < f->list.count; p++) {
            AstNode *param = f->list.items[p];
            AstNode *v = ast_new(AST_VAR_DECL, line);
            v->str1 = param->str1;
            v->type = param->type;
            if (p == 0) {
                v->a = ast_new(AST_INT_LIT, line);
                v->a->ival = 1;
            } else {
                const char *array = (p == 1) ? "__v32_argv" : "__v32_envp";
                ast_list_append(&body, main_string_array(array, p == 1 ? "program" : NULL, line));
                v->a = ast_ident(array, line);
            }
            ast_list_append(&body, v);
            if (p == 2) break;
        }
        for (int s = 0; s < f->a->list.count; s++) ast_list_append(&body, f->a->list.items[s]);
        f->a->list = body;
        f->list = ast_list_new();
    }
}

/* ---- variadic functions ---------------------------------------------------------
 *
 * Vircon32 C has no `...`. A variadic function's extra arguments travel as
 * ONE more argument, a pointer to an array of words holding them (the
 * parser gives every `...` function a last parameter `int *__v32_va`, and
 * turns va_start / va_arg / va_end into pointer arithmetic on it -- see
 * parser.y). What is left is each CALL:
 *
 *     msg("%s hits %d", name, dmg);
 * becomes
 *     int __v32_va_tmp0[2];                 -- at the top of the function
 *     ...
 *     __v32_va_tmp0[0] = (int) name;
 *     __v32_va_tmp0[1] = (int) dmg;
 *     msg("%s hits %d", __v32_va_tmp0);
 *
 * and a call with no extra arguments passes a null pointer. Each call
 * site has an array of its own, so calls nest (`sum(2, f("%d", 1), 3)`).
 *
 * The assignments go in front of the statement the call is in. In a loop
 * condition that would run them once instead of every time round, so
 * `while (f(...))` becomes `while (true) { <assignments> if (!(f(...)))
 * break; ... }` (`continue` still re-evaluates everything, as it must).
 * On the right of && or || they must only run when that side is evaluated
 * at all: see va_short_circuit. Not supported, and reported: a variadic
 * call with extra arguments in a do-while condition, in the step of a for
 * loop, or in a branch of ?:.
 */
typedef struct VaCtx {
    const NameSet *variadic;    /* name -> number of fixed parameters */
    AstList temps;              /* the function's argument arrays */
    int counter;
    int sc_counter;             /* flags for && and ||, see va_short_circuit */
    int errors;
} VaCtx;

static AstNode *va_int_lit(int value, int line) {
    AstNode *n = ast_new(AST_INT_LIT, line);
    n->ival = value;
    return n;
}

static void va_unsupported(VaCtx *cx, const AstNode *n, const char *where);
static void va_expr(AstNode **slot, VaCtx *cx, AstList *pre);

/* `left && right` / `left || right` where evaluating `right` needs
 * statements run first (it calls a variadic function): those statements
 * may only run when `right` itself would be evaluated. So the whole
 * expression becomes a flag, worked out by statements that go ahead of
 * the enclosing statement:
 *
 *     int __v32_sc_tmpN = 0;                    (1 for ||)
 *     if (left) {                               (if (!(left)) for ||)
 *         <the statements right needs>
 *         if (right) __v32_sc_tmpN = 1;         (if (!(right)) ... = 0)
 *     }
 */
static void va_short_circuit(AstNode **slot, VaCtx *cx, AstList *pre, AstList *right_pre) {
    AstNode *n = *slot;
    int line = n->line;
    int is_or = (strcmp(n->str1, "||") == 0);
    char name[40];
    snprintf(name, sizeof name, "__v32_sc_tmp%d", cx->sc_counter++);

    AstNode *flag = ast_new(AST_VAR_DECL, line);
    flag->str1 = strdup(name);
    flag->type = ast_ident("int", line);
    flag->a = va_int_lit(is_or, line);
    ast_list_append(pre, flag);

    AstNode *left = n->a, *right = n->b;
    if (is_or) {
        AstNode *not_left = ast_new(AST_UNOP, line);
        not_left->str1 = strdup("!");
        not_left->a = left;
        left = not_left;
        AstNode *not_right = ast_new(AST_UNOP, line);
        not_right->str1 = strdup("!");
        not_right->a = right;
        right = not_right;
    }
    AstNode *set = ast_new(AST_ASSIGN, line);
    set->str1 = strdup("=");
    set->a = ast_ident(name, line);
    set->b = va_int_lit(!is_or, line);
    AstNode *set_stmt = ast_new(AST_EXPR_STMT, line);
    set_stmt->a = set;
    AstNode *inner = ast_new(AST_IF, line);
    inner->a = right;
    inner->b = set_stmt;
    AstNode *block = ast_new(AST_BLOCK, line);
    block->list = *right_pre;
    ast_list_append(&block->list, inner);
    AstNode *outer = ast_new(AST_IF, line);
    outer->a = left;
    outer->b = block;
    ast_list_append(pre, outer);

    *slot = ast_ident(name, line);
}

static void va_expr(AstNode **slot, VaCtx *cx, AstList *pre) {
    AstNode *n = *slot;
    if (n == NULL || n->kind == AST_SIZEOF) return;
    if (n->kind == AST_BINOP && n->str1 != NULL &&
        (strcmp(n->str1, "&&") == 0 || strcmp(n->str1, "||") == 0)) {
        AstList right_pre = ast_list_new();
        va_expr(&n->a, cx, pre);
        va_expr(&n->b, cx, &right_pre);
        if (right_pre.count > 0) va_short_circuit(slot, cx, pre, &right_pre);
        return;
    }
    if (n->kind == AST_TERNARY) {
        AstList branch_pre = ast_list_new();
        va_expr(&n->a, cx, pre);
        va_expr(&n->b, cx, &branch_pre);
        va_expr(&n->c, cx, &branch_pre);
        if (branch_pre.count > 0) va_unsupported(cx, n, "a branch of ?:");
        return;
    }
    va_expr(&n->a, cx, pre);
    va_expr(&n->b, cx, pre);
    va_expr(&n->c, cx, pre);
    va_expr(&n->d, cx, pre);
    for (int i = 0; i < n->list.count; i++) va_expr(&n->list.items[i], cx, pre);

    if (n->kind != AST_CALL || n->a == NULL || n->a->kind != AST_IDENT) return;
    int at = name_set_find(cx->variadic, n->a->str1);
    if (at < 0) return;
    int fixed = cx->variadic->values[at];
    int extra = n->list.count - fixed;
    if (extra < 0) return;                      /* too few: sema reports it */
    int line = n->line;
    if (extra == 0) {
        ast_list_append(&n->list, ast_new(AST_NULL_LIT, line));
        return;
    }
    char name[40];
    snprintf(name, sizeof name, "__v32_va_tmp%d", cx->counter++);
    AstNode *temp = ast_new(AST_VAR_DECL, line);
    temp->str1 = strdup(name);
    temp->type = ast_wrap_array(ast_ident("int", line), extra, line);
    ast_list_append(&cx->temps, temp);
    for (int i = 0; i < extra; i++) {
        AstNode *element = ast_new(AST_SUBSCRIPT, line);
        element->a = ast_ident(name, line);
        element->b = va_int_lit(i, line);
        AstNode *value = ast_new(AST_CAST, line);
        value->type = ast_ident("int", line);
        value->a = n->list.items[fixed + i];
        AstNode *assign = ast_new(AST_ASSIGN, line);
        assign->str1 = strdup("=");
        assign->a = element;
        assign->b = value;
        AstNode *stmt = ast_new(AST_EXPR_STMT, line);
        stmt->a = assign;
        ast_list_append(pre, stmt);
    }
    n->list.count = fixed;
    ast_list_append(&n->list, ast_ident(name, line));
}

static void va_stmt(AstNode **slot, VaCtx *cx, AstList *pre);

static void va_stmt_list(AstList *list, VaCtx *cx) {
    AstList out = ast_list_new();
    for (int i = 0; i < list->count; i++) {
        AstList pre = ast_list_new();
        va_stmt(&list->items[i], cx, &pre);
        for (int j = 0; j < pre.count; j++) ast_list_append(&out, pre.items[j]);
        ast_list_append(&out, list->items[i]);
    }
    *list = out;
}

/* A statement that is not in a list (the body of an `if`): what must run
 * before it goes into a block with it. */
static void va_child(AstNode **slot, VaCtx *cx) {
    if (*slot == NULL) return;
    AstList pre = ast_list_new();
    va_stmt(slot, cx, &pre);
    if (pre.count == 0) return;
    AstNode *block = ast_new(AST_BLOCK, (*slot)->line);
    block->list = pre;
    ast_list_append(&block->list, *slot);
    *slot = block;
}

/* `while (cond)` -> `while (true) { pre; if (!(cond)) break; body }` */
static void va_loop_condition(AstNode **cond, AstNode **body, AstList *pre) {
    int line = (*cond)->line;
    AstNode *not_cond = ast_new(AST_UNOP, line);
    not_cond->str1 = strdup("!");
    not_cond->a = *cond;
    AstNode *leave = ast_new(AST_IF, line);
    leave->a = not_cond;
    leave->b = ast_new(AST_BREAK, line);
    AstNode *block = ast_new(AST_BLOCK, line);
    block->list = *pre;
    ast_list_append(&block->list, leave);
    if (*body != NULL) ast_list_append(&block->list, *body);
    *body = block;
    *cond = ast_new(AST_BOOL_LIT, line);
    (*cond)->ival = 1;
}

static void va_unsupported(VaCtx *cx, const AstNode *n, const char *where) {
    fprintf(stderr, "%s:%d: error: a call of a variadic function with extra arguments "
            "is not supported in %s -- give the call a statement of its own\n",
            n->file ? n->file : g_current_filename, n->line, where);
    cx->errors++;
}

static void va_stmt(AstNode **slot, VaCtx *cx, AstList *pre) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            va_stmt_list(&n->list, cx);
            break;
        case AST_EXPR_STMT:
        case AST_RETURN:
        case AST_VAR_DECL:
            va_expr(&n->a, cx, pre);
            break;
        case AST_IF:
            va_expr(&n->a, cx, pre);
            va_child(&n->b, cx);
            va_child(&n->c, cx);
            break;
        case AST_SWITCH:
            va_expr(&n->a, cx, pre);
            /* its statements stand between case labels: each keeps what
             * it needs in a block of its own */
            for (int i = 0; i < n->list.count; i++) va_child(&n->list.items[i], cx);
            break;
        case AST_WHILE: {
            AstList cond_pre = ast_list_new();
            va_expr(&n->a, cx, &cond_pre);
            va_child(&n->b, cx);
            if (cond_pre.count > 0) {
                if (n->ival == 1) va_unsupported(cx, n, "a do-while condition");
                else va_loop_condition(&n->a, &n->b, &cond_pre);
            }
            break;
        }
        case AST_FOR: {
            AstList cond_pre = ast_list_new(), step_pre = ast_list_new();
            va_stmt(&n->a, cx, pre);
            va_expr(&n->b, cx, &cond_pre);
            va_expr(&n->c, cx, &step_pre);
            va_child(&n->d, cx);
            if (step_pre.count > 0) va_unsupported(cx, n, "the step of a for loop");
            if (cond_pre.count > 0) va_loop_condition(&n->b, &n->d, &cond_pre);
            break;
        }
        case AST_LABEL:
            va_child(&n->a, cx);
            break;
        default:
            break;
    }
}

int cmode_rewrite_variadics(AstList *decls) {
    NameSet variadic = { NULL, NULL, 0, 0 };
    for (int i = 0; i < decls->count; i++) {
        AstNode *f = decls->items[i];
        if (!is_function(f) || f->list.count == 0) continue;
        AstNode *last = f->list.items[f->list.count - 1];
        if (last->str2 != NULL && strcmp(last->str2, "...") == 0)
            name_set_add(&variadic, f->str1, f->list.count - 1);
    }
    int errors = 0;
    if (variadic.count > 0) {
        for (int i = 0; i < decls->count; i++) {
            AstNode *f = decls->items[i];
            if (f->kind != AST_FUNC_DEF || f->a == NULL || f->a->kind != AST_BLOCK) continue;
            VaCtx cx = { &variadic, ast_list_new(), 0, 0, 0 };
            va_stmt_list(&f->a->list, &cx);
            errors += cx.errors;
            if (cx.temps.count == 0) continue;
            AstList body = cx.temps;
            for (int s = 0; s < f->a->list.count; s++) ast_list_append(&body, f->a->list.items[s]);
            f->a->list = body;
        }
    }
    name_set_free(&variadic);
    return errors;
}
