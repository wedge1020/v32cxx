#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ast.h"

AstList ast_list_new(void) {
    AstList l;
    l.items = NULL;
    l.count = 0;
    l.capacity = 0;
    return l;
}

void ast_list_append(AstList *list, AstNode *node) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 4;
        list->items = realloc(list->items, sizeof(AstNode *) * (size_t)list->capacity);
    }
    list->items[list->count++] = node;
}

/* Like ast_list_append, except an AST_VAR_DECL_GROUP node (see its own
 * doc comment in ast.h) is expanded into its own several entries instead
 * of being appended as one -- the multi-declarator statement support
 * var_decl's own plain-declarator production adds (parser.y) needs every
 * caller that folds a var_decl into a surrounding list to use this
 * instead of a plain ast_list_append, specifically so `int a, b, c;`
 * lands as three separate AST_VAR_DECL entries in that list, not one
 * mis-shapen node wrapping three. A strict no-op passthrough to the
 * plain function above for anything that isn't a group -- every existing
 * caller of the ordinary ast_list_append this replaces sees zero
 * behavior change for every node kind it already handled. */
void ast_list_append_flatten(AstList *list, AstNode *node) {
    if (node != NULL && node->kind == AST_VAR_DECL_GROUP) {
        for (int i = 0; i < node->list.count; i++) {
            ast_list_append(list, node->list.items[i]);
        }
    } else {
        ast_list_append(list, node);
    }
}

extern const char *g_current_filename;   /* driver.h; kept current by lexer.l */

AstNode *ast_new(AstKind kind, int line) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->kind = kind;
    n->line = line;
    n->file = g_current_filename;
    n->list = ast_list_new();
    return n;
}

AstNode *ast_ident(const char *name, int line) {
    AstNode *n = ast_new(AST_IDENT, line);
    n->str1 = strdup(name);
    return n;
}

AstNode *ast_wrap_pointer(AstNode *inner, int line) {
    AstNode *n = ast_new(AST_POINTER_TYPE, line);
    n->a = inner;
    return n;
}

AstNode *ast_wrap_reference(AstNode *inner, int line) {
    AstNode *n = ast_new(AST_REFERENCE_TYPE, line);
    n->a = inner;
    return n;
}

AstNode *ast_wrap_const(AstNode *inner, int line) {
    AstNode *n = ast_new(AST_CONST_TYPE, line);
    n->a = inner;
    return n;
}

AstNode *ast_wrap_array(AstNode *inner, int length, int line) {
    AstNode *n = ast_new(AST_ARRAY_TYPE, line);
    n->a = inner;
    n->ival = length;
    return n;
}

AstNode *ast_wrap_array_dims(AstNode *inner, AstList dims, int line) {
    /* dims holds one AST_INT_LIT per bracket group, in SOURCE order
     * (left to right, e.g. [8][4] -> {8, 4}) -- real C's own multi-
     * dimensional array semantics need these wrapped from the LAST
     * dimension inward, since the type of `grid` in `int grid[8][4]`
     * is "array of 8 (array of 4 int)", the OUTERMOST AST_ARRAY_TYPE
     * carrying the FIRST bracket's own length, not the last. Walking
     * the list backwards and calling the existing, single-dimension
     * ast_wrap_array repeatedly builds that nesting directly, with no
     * new AST node kind needed at all -- a 2D array is simply two
     * ordinary AST_ARRAY_TYPE nodes, one wrapping the other, the exact
     * same shape this project already uses for "array of function
     * pointers" (an AST_ARRAY_TYPE wrapping an AST_FUNC_PTR_TYPE). */
    AstNode *result = inner;
    for (int i = dims.count - 1; i >= 0; i--) {
        result = ast_wrap_array(result, dims.items[i]->ival, line);
        /* parser.y's array_dim: the item's `a` is the dimension's own
         * source expression (NULL for a bare literal) -- see
         * AST_ARRAY_TYPE's `b` in ast.h. */
        result->b = dims.items[i]->a;
    }
    return result;
}

int ast_fold_int(const AstNode *e, int (*lookup)(const char *name, int *value), int *out) {
    if (e == NULL) return 0;
    int a, b, c;
    switch (e->kind) {
        case AST_INT_LIT:
        case AST_CHAR_LIT:
        case AST_BOOL_LIT:
            *out = e->ival;
            return 1;
        case AST_IDENT:
            return lookup != NULL && e->str1 != NULL && lookup(e->str1, out);
        case AST_UNOP:
            if (!ast_fold_int(e->a, lookup, &a)) return 0;
            if (strcmp(e->str1, "neg") == 0) { *out = -a; return 1; }
            if (strcmp(e->str1, "~") == 0)   { *out = ~a; return 1; }
            if (strcmp(e->str1, "!") == 0)   { *out = !a; return 1; }
            if (strcmp(e->str1, "+") == 0)   { *out = a;  return 1; }
            return 0;
        case AST_TERNARY:
            if (!ast_fold_int(e->a, lookup, &c)) return 0;
            return ast_fold_int(c ? e->b : e->c, lookup, out);
        case AST_BINOP: {
            if (!ast_fold_int(e->a, lookup, &a) || !ast_fold_int(e->b, lookup, &b)) return 0;
            const char *op = e->str1;
            if (strcmp(op, "+") == 0)  { *out = a + b; return 1; }
            if (strcmp(op, "-") == 0)  { *out = a - b; return 1; }
            if (strcmp(op, "*") == 0)  { *out = a * b; return 1; }
            if (strcmp(op, "/") == 0)  { if (b == 0) return 0; *out = a / b; return 1; }
            if (strcmp(op, "%") == 0)  { if (b == 0) return 0; *out = a % b; return 1; }
            if (strcmp(op, "<<") == 0) { *out = a << b; return 1; }
            if (strcmp(op, ">>") == 0) { *out = a >> b; return 1; }
            if (strcmp(op, "&") == 0)  { *out = a & b; return 1; }
            if (strcmp(op, "|") == 0)  { *out = a | b; return 1; }
            if (strcmp(op, "^") == 0)  { *out = a ^ b; return 1; }
            if (strcmp(op, "<") == 0)  { *out = a < b; return 1; }
            if (strcmp(op, ">") == 0)  { *out = a > b; return 1; }
            if (strcmp(op, "<=") == 0) { *out = a <= b; return 1; }
            if (strcmp(op, ">=") == 0) { *out = a >= b; return 1; }
            if (strcmp(op, "==") == 0) { *out = a == b; return 1; }
            if (strcmp(op, "!=") == 0) { *out = a != b; return 1; }
            if (strcmp(op, "&&") == 0) { *out = a && b; return 1; }
            if (strcmp(op, "||") == 0) { *out = a || b; return 1; }
            return 0;
        }
        default:
            return 0;
    }
}

int ast_dim_expr_printable(const AstNode *e) {
    if (e == NULL) return 0;
    switch (e->kind) {
        case AST_INT_LIT:
        case AST_CHAR_LIT:
            return 1;
        case AST_UNOP:
            return (strcmp(e->str1, "neg") == 0 || strcmp(e->str1, "~") == 0 ||
                    strcmp(e->str1, "!") == 0) && ast_dim_expr_printable(e->a);
        case AST_BINOP:
            return ast_dim_expr_printable(e->a) && ast_dim_expr_printable(e->b);
        default:
            return 0;
    }
}

AstNode *ast_wrap_func_ptr(AstNode *return_type, AstList param_types, int line) {
    AstNode *n = ast_new(AST_FUNC_PTR_TYPE, line);
    n->type = return_type;
    n->list = param_types;
    return n;
}

static const char *kind_name(AstKind k) {
    switch (k) {
        case AST_PROGRAM: return "Program";
        case AST_NAMESPACE_DECL: return "NamespaceDecl";
        case AST_CLASS_DECL: return "ClassDecl";
        case AST_ACCESS_SPEC: return "AccessSpec";
        case AST_VAR_DECL: return "VarDecl";
        case AST_VAR_DECL_GROUP: return "VarDeclGroup"; /* should never
            actually be dumped -- flattened away by ast_list_append_flatten
            before it ever lands in a list a dump would walk -- named here
            anyway so a bug that DID let one survive would be obvious
            (a labeled, if unexpected, node) rather than falling through to
            this function's own "?" fallback for a truly unknown kind */
        case AST_TYPEDEF_DECL: return "TypedefDecl";
        case AST_NATIVE_DECL:  return "NativeDecl";
        case AST_ENUM_DECL: return "EnumDecl";
        case AST_ENUM_VALUE: return "EnumValue";
        case AST_UNION_DECL: return "UnionDecl";
        case AST_FUNC_DECL: return "FuncDecl";
        case AST_FUNC_DEF: return "FuncDef";
        case AST_PARAM: return "Param";
        case AST_MEMBER_INIT_LIST: return "MemberInitList";
        case AST_MEMBER_INIT: return "MemberInit";
        case AST_BLOCK: return "Block";
        case AST_IF: return "If";
        case AST_WHILE: return "While";
        case AST_FOR: return "For";
        case AST_RETURN: return "Return";
        case AST_BREAK: return "Break";
        case AST_CONTINUE: return "Continue";
        case AST_GOTO: return "Goto";
        case AST_LABEL: return "Label";
        case AST_ASM: return "Asm";  /* list=one StringLit per asm
            string literal; ival=0 brace form, 1 GCC parenthesized
            form -- see AST_ASM's own doc comment in ast.h */
        case AST_SWITCH: return "Switch";
        case AST_CASE: return "Case";
        case AST_DEFAULT: return "Default";
        case AST_EXPR_STMT: return "ExprStmt";
        case AST_BINOP: return "BinOp";
        case AST_UNOP: return "UnOp";
        case AST_ASSIGN: return "Assign";
        case AST_TERNARY: return "Ternary";
        case AST_CALL: return "Call";
        case AST_MEMBER: return "Member";
        case AST_SUBSCRIPT: return "Subscript";
        case AST_IDENT: return "Ident";
        case AST_QUALIFIED_ID: return "QualifiedId";
        case AST_INT_LIT: return "IntLit";
        case AST_FLOAT_LIT: return "FloatLit";
        case AST_STRING_LIT: return "StringLit";
        case AST_CHAR_LIT: return "CharLit";
        case AST_BOOL_LIT: return "BoolLit";
        case AST_NULL_LIT: return "NullLit";
        case AST_THIS: return "This";
        case AST_NEW: return "New";
        case AST_DIRECT_INIT: return "DirectInit";
        case AST_DELETE: return "Delete";
        case AST_POINTER_TYPE: return "PointerType";
        case AST_REFERENCE_TYPE: return "ReferenceType";
        case AST_CONST_TYPE: return "ConstType";
        case AST_ARRAY_TYPE: return "ArrayType";
        case AST_FUNC_PTR_TYPE: return "FuncPtrType";
        case AST_INIT_LIST: return "InitList";
        case AST_CAST: return "Cast";
        case AST_SIZEOF: return "Sizeof";
        case AST_FRIEND_CLASS: return "FriendClass";
        case AST_FRIEND_FUNC_DECL: return "FriendFuncDecl";
    }
    return "?";
}

static void indent_line(int indent) {
    for (int i = 0; i < indent; i++) fputs("  ", stdout);
}

void ast_dump(const AstNode *node, int indent) {
    if (node == NULL) {
        indent_line(indent);
        printf("(null)\n");
        return;
    }

    indent_line(indent);
    printf("%s", kind_name(node->kind));
    if (node->str1) printf(" str1=%s", node->str1);
    if (node->str2) printf(" str2=%s", node->str2);
    if (node->kind == AST_INT_LIT || node->kind == AST_CHAR_LIT || node->kind == AST_BOOL_LIT)
        printf(" ival=%d", node->ival);
    if (node->kind == AST_FLOAT_LIT)
        printf(" fval=%g", node->fval);
    printf(" @line%d\n", node->line);

    if (node->type) {
        indent_line(indent + 1);
        printf("type:\n");
        ast_dump(node->type, indent + 2);
    }
    if (node->a) { indent_line(indent + 1); printf("a:\n"); ast_dump(node->a, indent + 2); }
    if (node->b) { indent_line(indent + 1); printf("b:\n"); ast_dump(node->b, indent + 2); }
    if (node->c) { indent_line(indent + 1); printf("c:\n"); ast_dump(node->c, indent + 2); }
    if (node->d) { indent_line(indent + 1); printf("d:\n"); ast_dump(node->d, indent + 2); }
    for (int i = 0; i < node->list.count; i++) {
        indent_line(indent + 1);
        printf("[%d]:\n", i);
        ast_dump(node->list.items[i], indent + 2);
    }
}

int ast_decode_escape(const char **p) {
    const char *s = *p;
    int v;
    if (*s >= '0' && *s <= '7') {
        v = 0;
        for (int i = 0; i < 3 && *s >= '0' && *s <= '7'; i++) v = v * 8 + (*s++ - '0');
        *p = s;
        return v & 0xFF;
    }
    if (*s == 'x' || *s == 'X') {
        const char *h = s + 1;
        if (!isxdigit((unsigned char)*h)) { *p = s + 1; return 'x'; }
        v = 0;
        while (isxdigit((unsigned char)*h)) {
            int d = isdigit((unsigned char)*h) ? *h - '0' : (tolower((unsigned char)*h) - 'a' + 10);
            v = (v * 16 + d) & 0xFFFF;
            h++;
        }
        *p = h;
        return v & 0xFF;
    }
    *p = s + 1;
    switch (*s) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        default:  return (unsigned char)*s;   /* \\ \' \" \? and unknown */
    }
}

/* ---- post-parse rewrites for C storage classes -------------------------
 *
 * Both run from main.c right after a successful parse, before sema, so
 * every later phase only ever sees ordinary globals.
 */

/* Replaces every use of the variable `old_name` under `n` with `new_name`,
 * honouring shadowing: a nested block that declares its own `old_name`
 * keeps it from that declaration on. Returns 1 if `n` itself was such a
 * shadowing declaration (the caller stops renaming the rest of its block). */
static int rename_var_uses(AstNode *n, const char *old_name, const char *new_name);

static void rename_in_list(AstList *list, int from, const char *old_name, const char *new_name) {
    for (int i = from; i < list->count; i++) {
        if (rename_var_uses(list->items[i], old_name, new_name)) break;
    }
}

static int rename_var_uses(AstNode *n, const char *old_name, const char *new_name) {
    if (n == NULL) return 0;
    switch (n->kind) {
        case AST_IDENT:
            if (n->str1 != NULL && strcmp(n->str1, old_name) == 0) {
                n->str1 = strdup(new_name);
            }
            return 0;
        case AST_VAR_DECL:
            rename_var_uses(n->a, old_name, new_name);
            return n->str1 != NULL && strcmp(n->str1, old_name) == 0;
        case AST_BLOCK:
        case AST_SWITCH:
            if (n->kind == AST_SWITCH) rename_var_uses(n->a, old_name, new_name);
            rename_in_list(&n->list, 0, old_name, new_name);
            return 0;
        case AST_FOR:
            /* `for (int x = ...; ...)` shadows for the whole statement */
            if (rename_var_uses(n->a, old_name, new_name)) return 0;
            rename_var_uses(n->b, old_name, new_name);
            rename_var_uses(n->c, old_name, new_name);
            rename_var_uses(n->d, old_name, new_name);
            return 0;
        case AST_MEMBER:
        case AST_CAST:
        case AST_SIZEOF:
            /* never `type`, and never str2 (a member's own name) */
            rename_var_uses(n->a, old_name, new_name);
            return 0;
        default:
            rename_var_uses(n->a, old_name, new_name);
            rename_var_uses(n->b, old_name, new_name);
            rename_var_uses(n->c, old_name, new_name);
            rename_var_uses(n->d, old_name, new_name);
            for (int i = 0; i < n->list.count; i++) {
                rename_var_uses(n->list.items[i], old_name, new_name);
            }
            return 0;
    }
}

static int g_static_local_counter = 0;

/* Finds the `static` locals under statement `n` (which belongs to function
 * `func_name`), renames each to a program-unique global name, and moves
 * its declaration to `hoisted`. */
static void hoist_statics_in(AstNode *n, const char *func_name, AstList *hoisted) {
    if (n == NULL) return;
    if (n->kind == AST_BLOCK || n->kind == AST_SWITCH) {
        AstList kept = ast_list_new();
        for (int i = 0; i < n->list.count; i++) {
            AstNode *st = n->list.items[i];
            if (st != NULL && st->kind == AST_VAR_DECL && st->is_static_local) {
                char buf[256];
                snprintf(buf, sizeof buf, "__static%d_%s_%s",
                         g_static_local_counter++, func_name, st->str1);
                /* the initializer is evaluated once, at file scope: it
                 * cannot see the local's own name, so no rename there */
                rename_in_list(&n->list, i + 1, st->str1, buf);
                st->str1 = strdup(buf);
                st->is_static_local = 0;
                ast_list_append(hoisted, st);
            } else {
                hoist_statics_in(st, func_name, hoisted);
                ast_list_append(&kept, st);
            }
        }
        n->list = kept;
        return;
    }
    switch (n->kind) {
        case AST_IF:    hoist_statics_in(n->b, func_name, hoisted);
                        hoist_statics_in(n->c, func_name, hoisted); break;
        case AST_WHILE: hoist_statics_in(n->b, func_name, hoisted); break;
        case AST_FOR:   hoist_statics_in(n->d, func_name, hoisted); break;
        case AST_LABEL: hoist_statics_in(n->a, func_name, hoisted); break;
        default: break;
    }
}

/* `static` locals -> uniquely named file-scope variables, declared just
 * ahead of the function that owns them (or of its class). Vircon32 C has
 * no `static`; a global has the same lifetime, and the generated name
 * keeps it as private as the local was. */
void hoist_static_locals(AstList *decls) {
    AstList out = ast_list_new();
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        AstList hoisted = ast_list_new();
        if (n->kind == AST_FUNC_DEF) {
            hoist_statics_in(n->a, n->str1 ? n->str1 : "fn", &hoisted);
        } else if (n->kind == AST_CLASS_DECL) {
            for (int j = 0; j < n->list.count; j++) {
                AstNode *m = n->list.items[j];
                if (m != NULL && m->kind == AST_FUNC_DEF) {
                    hoist_statics_in(m->a, m->str1 ? m->str1 : n->str1, &hoisted);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            hoist_static_locals(&n->list);
        }
        for (int j = 0; j < hoisted.count; j++) ast_list_append(&out, hoisted.items[j]);
        ast_list_append(&out, n);
    }
    *decls = out;
}

/* C lets a file-scope variable be declared more than once -- `extern int
 * x;` ahead of `int x = 5;`, or a plain tentative `int x;` repeated --
 * and it is still one object. Vircon32 C wants exactly one definition,
 * so keep one declaration per name: the one with an initializer if there
 * is one, otherwise the first. */
void merge_tentative_globals(AstList *decls) {
    AstList out = ast_list_new();
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) merge_tentative_globals(&n->list);
        if (n->kind == AST_VAR_DECL && n->str1 != NULL) {
            int drop = 0;
            for (int j = 0; j < decls->count && !drop; j++) {
                AstNode *o = decls->items[j];
                if (j == i || o->kind != AST_VAR_DECL || o->str1 == NULL ||
                    strcmp(o->str1, n->str1) != 0) continue;
                if (n->a == NULL && (o->a != NULL || j < i)) drop = 1;
            }
            if (drop) continue;
        }
        ast_list_append(&out, n);
    }
    *decls = out;
}

/* ---- implicit constructors and destructors ------------------------------
 *
 * C++ gives a class that declares no constructor an implicit default one,
 * which constructs its base and its members; likewise for the destructor.
 * Everything downstream here hangs construction off a constructor that
 * EXISTS (base-constructor calls, member construction and vtable setup are
 * injected into constructor bodies; scope exit, delete and arrays call a
 * destructor that has a body), so a class like
 *
 *     class Fast : public Body { public: virtual Vec step(); };
 *
 * used to get neither: `Fast f;` never ran Body(), and leaving scope never
 * ran ~Body(). Rather than teach every phase about "a class with no
 * constructor", the missing members are written into the class right
 * after the parse, as if the source had said `Fast() { }` / `~Fast() { }`,
 * and the existing machinery does the rest.
 *
 * Only where it changes something: a constructor when the base or a
 * by-value member has a default constructor (or a member has virtual
 * methods, so its vtable pointer needs setting); a destructor when the
 * base or a by-value member has one. A plain struct gets nothing.
 */
typedef struct ClassList { AstNode **items; int count, cap; } ClassList;

static void collect_classes(AstList *decls, ClassList *out) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_NAMESPACE_DECL) collect_classes(&n->list, out);
        if (n->kind != AST_CLASS_DECL) continue;
        if (out->count == out->cap) {
            out->cap = out->cap ? out->cap * 2 : 16;
            out->items = realloc(out->items, sizeof(AstNode *) * out->cap);
        }
        out->items[out->count++] = n;
    }
}

static AstNode *class_named(const ClassList *all, const char *name) {
    if (name == NULL) return NULL;
    for (int i = 0; i < all->count; i++)
        if (strcmp(all->items[i]->str1, name) == 0) return all->items[i];
    return NULL;
}

static int is_func(const AstNode *m) {
    return m != NULL && (m->kind == AST_FUNC_DECL || m->kind == AST_FUNC_DEF) && m->str1 != NULL;
}

static int class_declares_ctor(const AstNode *c) {
    for (int i = 0; i < c->list.count; i++)
        if (is_func(c->list.items[i]) && strcmp(c->list.items[i]->str1, c->str1) == 0) return 1;
    return 0;
}

static int class_has_default_ctor(const AstNode *c) {
    for (int i = 0; i < c->list.count; i++) {
        const AstNode *m = c->list.items[i];
        if (!is_func(m) || strcmp(m->str1, c->str1) != 0) continue;
        int all_defaulted = 1;
        for (int p = 0; p < m->list.count; p++)
            if (m->list.items[p]->a == NULL) { all_defaulted = 0; break; }
        if (all_defaulted) return 1;
    }
    return 0;
}

static AstNode *class_dtor(const AstNode *c) {
    for (int i = 0; i < c->list.count; i++)
        if (is_func(c->list.items[i]) && c->list.items[i]->str1[0] == '~') return c->list.items[i];
    return NULL;
}

static int class_has_virtuals(const ClassList *all, const AstNode *c) {
    for (int depth = 0; c != NULL && depth < 64; depth++) {
        for (int i = 0; i < c->list.count; i++)
            if (is_func(c->list.items[i]) && c->list.items[i]->ival) return 1;
        c = class_named(all, c->str2);
    }
    return 0;
}

/* The class of a by-value member, or of the elements of a one-dimensional
 * member ARRAY (`Counter items[3];`) -- the same members constructor
 * injection handles -- or NULL. Not a pointer or reference. */
static AstNode *member_value_class(const ClassList *all, const AstNode *m) {
    if (m == NULL || m->kind != AST_VAR_DECL || m->type == NULL) return NULL;
    const AstNode *t = m->type;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    if (t != NULL && t->kind == AST_ARRAY_TYPE) t = t->a;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    if (t == NULL || t->kind != AST_IDENT) return NULL;
    return class_named(all, t->str1);
}

static AstNode *make_implicit_member(const AstNode *c, int is_dtor) {
    AstNode *f = ast_new(AST_FUNC_DEF, c->line);
    if (is_dtor) {
        size_t len = strlen(c->str1) + 2;
        f->str1 = malloc(len);
        snprintf(f->str1, len, "~%s", c->str1);
    } else {
        f->str1 = strdup(c->str1);
    }
    f->type = NULL;
    f->a = ast_new(AST_BLOCK, c->line);
    f->access = ACC_PUBLIC;
    return f;
}

void synthesize_implicit_members(AstList *decls) {
    ClassList all = { NULL, 0, 0 };
    collect_classes(decls, &all);
    /* in declaration order: a base is always complete, implicit members
     * included, before a class derived from it or holding one is looked at */
    for (int i = 0; i < all.count; i++) {
        AstNode *c = all.items[i];
        AstNode *base = class_named(&all, c->str2);
        int want_ctor = 0, want_dtor = 0, base_dtor_virtual = 0;
        if (base != NULL && base != c) {
            if (class_has_default_ctor(base)) want_ctor = 1;
            AstNode *bd = class_dtor(base);
            if (bd != NULL) { want_dtor = 1; base_dtor_virtual = bd->ival; }
        }
        for (int j = 0; j < c->list.count; j++) {
            AstNode *mc = member_value_class(&all, c->list.items[j]);
            if (mc == NULL || mc == c) continue;
            if (class_has_default_ctor(mc) ||
                (!class_declares_ctor(mc) && class_has_virtuals(&all, mc))) want_ctor = 1;
            if (class_dtor(mc) != NULL) want_dtor = 1;
        }
        if (class_declares_ctor(c)) want_ctor = 0;
        if (class_dtor(c) != NULL) want_dtor = 0;
        if (!want_ctor && !want_dtor) continue;
        AstNode *pub = ast_new(AST_ACCESS_SPEC, c->line);
        pub->access = ACC_PUBLIC;
        ast_list_append(&c->list, pub);
        if (want_ctor) ast_list_append(&c->list, make_implicit_member(c, 0));
        if (want_dtor) {
            AstNode *d = make_implicit_member(c, 1);
            d->ival = base_dtor_virtual; /* virtual if the base's is */
            ast_list_append(&c->list, d);
        }
    }
    free(all.items);
}

/* ---- unnamed objects: `Enemy(1, 2, 3)` as an expression ------------------
 *
 * C++ lets a class name be used like a function to build an object with
 * no name: `v.push_back(Enemy(1, 2, 3));`, `return Vec(x, y);`. Nothing
 * downstream knows about objects without names, and nothing needs to:
 * this pass, run right after parsing, gives each one a name and declares
 * it as an ordinary local just ahead of the statement that uses it.
 *
 *     v.push_back(Enemy(1, 2, 3));        {
 *                                             Enemy __v32_obj0(1, 2, 3);
 *                                             v.push_back(__v32_obj0);
 *                                         }
 *
 * From there on it IS a local: its constructor is resolved and called,
 * it is passed by address to a reference parameter, and its destructor
 * runs when the block closes -- all by the code that already does those
 * things. The block is what gives it a temporary's lifetime (the end of
 * the statement). Three statements are handled differently:
 *
 *   - `Enemy e = Enemy(1, 2, 3);` becomes `Enemy e(1, 2, 3);` -- no
 *     second object at all, which is also what a C++ compiler does;
 *   - any other declaration keeps its place in the block, with the
 *     unnamed object declared before it (wrapping it in a block of its
 *     own would end the declared variable's life too);
 *   - a loop's condition or step is evaluated many times, and a
 *     declaration ahead of the loop would run once: reported as an error.
 *
 * One known difference from C++: in `a && f(Enemy(1))` the object is
 * built even when `a` is false.
 *
 * A typedef or enum name used the same way is a cast (`Fixed(3)` is
 * `(Fixed)3`) and is rewritten as one.
 */
static int g_obj_counter = 0;
static int g_obj_errors = 0;

static int is_unnamed_object(const AstNode *n) {
    return n != NULL && n->kind == AST_DIRECT_INIT && n->str1 != NULL;
}

static int contains_unnamed_object(const AstNode *n) {
    if (n == NULL) return 0;
    if (is_unnamed_object(n)) return 1;
    if (contains_unnamed_object(n->a) || contains_unnamed_object(n->b) ||
        contains_unnamed_object(n->c) || contains_unnamed_object(n->d)) return 1;
    for (int i = 0; i < n->list.count; i++)
        if (contains_unnamed_object(n->list.items[i])) return 1;
    return 0;
}

static void obj_error(const AstNode *n, const char *why) {
    fprintf(stderr, "%s:%d: error: %s\n", n->file != NULL ? n->file : "?", n->line, why);
    g_obj_errors++;
}

/* `Enemy(args)` -> the initializer of a declaration: NULL for no
 * arguments (default construction), else a plain AST_DIRECT_INIT. */
static AstNode *object_initializer(AstNode *obj) {
    if (obj->list.count == 0) return NULL;
    AstNode *init = ast_new(AST_DIRECT_INIT, obj->line);
    init->file = obj->file;
    init->list = obj->list;
    return init;
}

/* Replaces every unnamed object under *slot (an expression), innermost
 * first, appending one declaration per object to `prefix`. */
static void lift_objects_expr(AstNode **slot, const ClassList *all, AstList *prefix) {
    AstNode *n = *slot;
    if (n == NULL) return;
    lift_objects_expr(&n->a, all, prefix);
    lift_objects_expr(&n->b, all, prefix);
    lift_objects_expr(&n->c, all, prefix);
    lift_objects_expr(&n->d, all, prefix);
    for (int i = 0; i < n->list.count; i++) lift_objects_expr(&n->list.items[i], all, prefix);
    if (!is_unnamed_object(n)) return;

    if (class_named(all, n->str1) == NULL) {
        /* a typedef, enum or union name: a function-style cast */
        if (n->list.count != 1) {
            obj_error(n, "a function-style cast takes exactly one value");
            return;
        }
        AstNode *cast = ast_new(AST_CAST, n->line);
        cast->file = n->file;
        cast->type = ast_ident(n->str1, n->line);
        cast->a = n->list.items[0];
        *slot = cast;
        return;
    }
    char name[64];
    snprintf(name, sizeof name, "__v32_obj%d", g_obj_counter++);
    AstNode *decl = ast_new(AST_VAR_DECL, n->line);
    decl->file = n->file;
    decl->str1 = strdup(name);
    decl->type = ast_ident(n->str1, n->line);
    decl->a = object_initializer(n);
    ast_list_append(prefix, decl);
    AstNode *use = ast_ident(name, n->line);
    use->file = n->file;
    *slot = use;
}

/* `T x = T(args);` -> `T x(args);` */
static void elide_copy_from_object(AstNode *decl, const ClassList *all, AstList *prefix) {
    if (decl->kind != AST_VAR_DECL || !is_unnamed_object(decl->a)) return;
    const AstNode *t = decl->type;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    if (t == NULL || t->kind != AST_IDENT || strcmp(t->str1, decl->a->str1) != 0) return;
    if (class_named(all, decl->a->str1) == NULL) return;
    AstNode *obj = decl->a;
    for (int i = 0; i < obj->list.count; i++) lift_objects_expr(&obj->list.items[i], all, prefix);
    decl->a = object_initializer(obj);
}

static void lift_objects_decl(AstNode *n, const ClassList *all, AstList *prefix) {
    if (n->kind == AST_VAR_DECL_GROUP) {
        for (int i = 0; i < n->list.count; i++) lift_objects_decl(n->list.items[i], all, prefix);
        return;
    }
    elide_copy_from_object(n, all, prefix);
    lift_objects_expr(&n->a, all, prefix);
}

static void desugar_objects_slot(AstNode **slot, const ClassList *all);

/* Handles the statement *slot; returns in `prefix` the declarations that
 * must come immediately before it. Nested statements are finished here. */
static void desugar_objects_stmt(AstNode **slot, const ClassList *all, AstList *prefix) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
        case AST_SWITCH: {
            if (n->kind == AST_SWITCH) lift_objects_expr(&n->a, all, prefix);
            AstList out = ast_list_new();
            for (int i = 0; i < n->list.count; i++) {
                AstNode *item = n->list.items[i];
                AstList before = ast_list_new();
                desugar_objects_stmt(&item, all, &before);
                if (before.count > 0 && item != NULL &&
                    item->kind != AST_VAR_DECL && item->kind != AST_VAR_DECL_GROUP) {
                    AstNode *wrap = ast_new(AST_BLOCK, item->line);
                    wrap->file = item->file;
                    wrap->list = before;
                    ast_list_append(&wrap->list, item);
                    ast_list_append(&out, wrap);
                } else {
                    for (int k = 0; k < before.count; k++) ast_list_append(&out, before.items[k]);
                    ast_list_append(&out, item);
                }
            }
            n->list = out;
            break;
        }
        case AST_IF:
            lift_objects_expr(&n->a, all, prefix);
            desugar_objects_slot(&n->b, all);
            desugar_objects_slot(&n->c, all);
            break;
        case AST_WHILE:
            if (contains_unnamed_object(n->a))
                obj_error(n, "an unnamed object in a loop condition is not supported "
                             "(it would have to be rebuilt on every pass): declare it as a variable");
            desugar_objects_slot(&n->b, all);
            break;
        case AST_FOR:
            if (n->a != NULL) {
                if (n->a->kind == AST_VAR_DECL || n->a->kind == AST_VAR_DECL_GROUP) lift_objects_decl(n->a, all, prefix);
                else lift_objects_expr(&n->a->a, all, prefix);
            }
            if (contains_unnamed_object(n->b) || contains_unnamed_object(n->c))
                obj_error(n, "an unnamed object in a loop condition or step is not supported "
                             "(it would have to be rebuilt on every pass): declare it as a variable");
            desugar_objects_slot(&n->d, all);
            break;
        case AST_LABEL:
            desugar_objects_slot(&n->a, all);
            break;
        case AST_VAR_DECL:
        case AST_VAR_DECL_GROUP:
            lift_objects_decl(n, all, prefix);
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
            lift_objects_expr(&n->a, all, prefix);
            break;
        default:
            break;
    }
}

/* A statement that is NOT an item of a block's list (an `if` branch, a
 * loop body): anything it needs declared goes into a block around it. */
static void desugar_objects_slot(AstNode **slot, const ClassList *all) {
    if (*slot == NULL) return;
    AstList before = ast_list_new();
    desugar_objects_stmt(slot, all, &before);
    if (before.count == 0) return;
    AstNode *wrap = ast_new(AST_BLOCK, (*slot)->line);
    wrap->file = (*slot)->file;
    wrap->list = before;
    ast_list_append(&wrap->list, *slot);
    *slot = wrap;
}

static void report_leftover_objects(const AstNode *n) {
    if (n == NULL) return;
    if (is_unnamed_object(n)) {
        obj_error(n, "an unnamed object can't be used here (a default argument, a member "
                     "initializer or a file-scope initializer): declare it as a variable");
        return;
    }
    report_leftover_objects(n->a);
    report_leftover_objects(n->b);
    report_leftover_objects(n->c);
    report_leftover_objects(n->d);
    for (int i = 0; i < n->list.count; i++) report_leftover_objects(n->list.items[i]);
}

static void desugar_objects_decls(AstList *decls, const ClassList *all) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n == NULL) continue;
        int errors_before = g_obj_errors;
        if (n->kind == AST_NAMESPACE_DECL || n->kind == AST_CLASS_DECL) {
            desugar_objects_decls(&n->list, all);
        } else if (n->kind == AST_FUNC_DEF) {
            g_obj_counter = 0;
            desugar_objects_slot(&n->a, all);
        } else if (n->kind == AST_VAR_DECL || n->kind == AST_VAR_DECL_GROUP) {
            /* file scope: only the `T g = T(args);` form has anywhere to go */
            AstList none = ast_list_new();
            if (n->kind == AST_VAR_DECL) elide_copy_from_object(n, all, &none);
        }
        if (g_obj_errors == errors_before) report_leftover_objects(n);
    }
}

int desugar_unnamed_objects(AstList *decls) {
    ClassList all = { NULL, 0, 0 };
    collect_classes(decls, &all);
    g_obj_errors = 0;
    desugar_objects_decls(decls, &all);
    free(all.items);
    return g_obj_errors;
}

AstNode *ast_clone_expr(const AstNode *n) {
    if (n == NULL) return NULL;
    AstNode *c = malloc(sizeof *c);
    *c = *n;
    if (n->str1 != NULL) c->str1 = strdup(n->str1);
    if (n->str2 != NULL) c->str2 = strdup(n->str2);
    if (n->macro_name != NULL) c->macro_name = strdup(n->macro_name);
    c->a = ast_clone_expr(n->a);
    c->b = ast_clone_expr(n->b);
    c->c = ast_clone_expr(n->c);
    c->d = ast_clone_expr(n->d);
    c->list = ast_list_new();
    for (int i = 0; i < n->list.count; i++) ast_list_append(&c->list, ast_clone_expr(n->list.items[i]));
    return c;
}
