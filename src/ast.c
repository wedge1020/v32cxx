#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ast.h"
#include "prescan.h"

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
    if ((kind == AST_FUNC_DEF || kind == AST_FUNC_DECL) && (g_attr_pending & ATTR_NO_YIELD)) {
        n->attr_flags |= ATTR_NO_YIELD;
        g_attr_pending &= ~ATTR_NO_YIELD;
    } else if (kind == AST_VAR_DECL && (g_attr_pending & ATTR_ROM)) {
        n->attr_flags |= ATTR_ROM;
        g_attr_pending &= ~ATTR_ROM;
    }
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

int (*g_ast_sizeof_hook)(const AstNode *sizeof_node, int *out) = NULL;

int ast_fold_int(const AstNode *e, int (*lookup)(const char *name, int *value), int *out) {
    if (e == NULL) return 0;
    int a, b, c;
    switch (e->kind) {
        case AST_SIZEOF:
            /* `char buf[sizeof table / sizeof table[0]]`: only whoever
             * knows the declarations so far can say (parser.y). */
            return g_ast_sizeof_hook != NULL && g_ast_sizeof_hook(e, out);
        case AST_INT_LIT:
        case AST_CHAR_LIT:
        case AST_BOOL_LIT:
            *out = e->ival;
            return 1;
        case AST_IDENT:
            return lookup != NULL && e->str1 != NULL && lookup(e->str1, out);
        case AST_QUALIFIED_ID: {
            /* `a::RED`, `a::E::RED`: the lookup gets the joined name */
            if (lookup == NULL) return 0;
            char buf[512];
            size_t used = 0;
            for (int i = 0; i < e->list.count; i++) {
                const char *part = e->list.items[i]->str1;
                if (part == NULL || used + strlen(part) + 3 >= sizeof buf) return 0;
                used += (size_t)snprintf(buf + used, sizeof buf - used, "%s%s", used ? "::" : "", part);
            }
            return lookup(buf, out);
        }
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
                    strcmp(e->str1, "+") == 0 || strcmp(e->str1, "!") == 0) && ast_dim_expr_printable(e->a);
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
        case AST_DESIG_INIT: return "DesigInit";
        case AST_COMPOUND_LIT: return "CompoundLit";
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
            if (st != NULL && st->kind == AST_VAR_DECL && st->is_static_local == 2) {
                /* local `extern T x;`: file scope, same name (parser.y) */
                st->is_static_local = 0;
                ast_list_append(hoisted, st);
            } else if (st != NULL && st->kind == AST_VAR_DECL && st->is_static_local) {
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
static int array_is_unsized(const AstNode *type) {
    return type != NULL && type->kind == AST_ARRAY_TYPE && type->ival < 0;
}

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
                /* `extern char prbuf[];` (C input): the declaration that
                 * gives the array its size is the one to keep. */
                int n_unsized = array_is_unsized(n->type), o_unsized = array_is_unsized(o->type);
                if (n_unsized != o_unsized) {
                    if (n_unsized) drop = 1;
                    continue;
                }
                if (n->a == NULL && (o->a != NULL || j < i)) drop = 1;
            }
            if (drop) continue;
            /* declared `extern` with no size and never defined: there is
             * no such array in this program */
            if (array_is_unsized(n->type) && n->is_extern) continue;
        }
        ast_list_append(&out, n);
    }
    *decls = out;
}

/* ---- `extern` variables across namespaces -----------------------------------
 *
 * Vircon32 C has no `extern` and no linker: a variable that is declared
 * `extern` and defined nowhere in the program IS the variable (it is printed
 * as an ordinary declaration, once), and one that is also defined is just
 * that definition. mark_extern_decls records which file/namespace-scope
 * variables were written `extern` (the C++ lexer drops the keyword: the
 * prescan reports it per declarator); merge_extern_names then makes sure an
 * `extern` declaration and the definition of the same C name are one object
 * even when they are in different namespace blocks:
 *
 *     namespace hw { extern int c_side; }    // C name `c_side`
 *     int c_side = 3;                        // the same variable
 *     namespace n { extern int v; }  namespace n { int v = 5; }   // n__v
 *
 * and that `a::x` / `b::x` both `extern` (both `x`) are one. merge_tentative_globals
 * already folds declarations that share one list. */
void mark_extern_decls(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_NAMESPACE_DECL) mark_extern_decls(&n->list);
        else if (n->kind == AST_VAR_DECL && n->str1 != NULL)
            n->extern_decl = n->is_extern || prescan_decl_is_extern(n->file, n->line, n->str1);
    }
}

/* ---- namespace-qualified out-of-line definitions -----------------------
 *
 * `int a::f(E e) {...}`, `void a::C::m() {...}`, `a::C::C(int x) : v(x) {}`,
 * `a::C::~C() {}`: a definition written at file scope whose qualifier chain
 * starts with namespace names. C++ treats the body as if it stood inside the
 * namespace (its names visible unqualified, default arguments from the
 * declaration), so that is what is built: the definition is wrapped, where it
 * stands, in `namespace a { ... }` (one block per qualifier, nested for
 * `a::b::f`), and the namespace names are dropped from its qualifier chain.
 * What is left of the chain (`C` of `a::C::m`) is an ordinary class-qualified
 * definition, which sema's attach_out_of_line resolves inside the block; a
 * chain that was all namespaces leaves a plain function definition. Reopening
 * a namespace is already supported, so nothing else needs to know.
 * Runs before anything walks the tree. */
static int ns_path_known(char **paths, int n, const char *p) {
    for (int i = 0; i < n; i++) if (strcmp(paths[i], p) == 0) return 1;
    return 0;
}

static void collect_ns_paths(AstList *decls, const char *prefix, char ***paths, int *n, int *cap) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d == NULL || d->kind != AST_NAMESPACE_DECL || d->str1 == NULL) continue;
        size_t len = strlen(prefix) + strlen(d->str1) + 3;
        char *full = malloc(len);
        snprintf(full, len, "%s%s%s", prefix, prefix[0] ? "::" : "", d->str1);
        if (!ns_path_known(*paths, *n, full)) {
            if (*n == *cap) { *cap = *cap ? *cap * 2 : 16; *paths = realloc(*paths, sizeof(char *) * (size_t)*cap); }
            (*paths)[(*n)++] = strdup(full);
        }
        collect_ns_paths(&d->list, full, paths, n, cap);
        free(full);
    }
}

/* Inside `namespace a::b`, `a::b::D` is `D`: a parameter or return type of a wrapped definition that spells
 * its own namespace out is written the way the declaration inside the namespace spells it, so the two
 * signatures compare equal (and mangle alike). */
static void strip_own_namespace(AstNode *t, AstNode **names, int nsn) {
    while (t != NULL) {
        if (t->kind == AST_CONST_TYPE || t->kind == AST_POINTER_TYPE || t->kind == AST_REFERENCE_TYPE ||
            t->kind == AST_ARRAY_TYPE) { t = t->a; continue; }
        if (t->kind == AST_QUALIFIED_ID && t->list.count > nsn) {
            for (int k = 0; k < nsn; k++)
                if (t->list.items[k]->str1 == NULL || strcmp(t->list.items[k]->str1, names[k]->str1) != 0) return;
            if (t->list.count - nsn == 1) {
                AstNode *last = t->list.items[t->list.count - 1];
                t->kind = AST_IDENT;
                t->str1 = strdup(last->str1);
                t->list.count = 0;
            } else {
                for (int k = nsn; k < t->list.count; k++) t->list.items[k - nsn] = t->list.items[k];
                t->list.count -= nsn;
            }
        }
        return;
    }
}

void qualify_namespace_definitions(AstList *decls) {
    char **paths = NULL;
    int n = 0, cap = 0;
    collect_ns_paths(decls, "", &paths, &n, &cap);
    for (int i = 0; n > 0 && i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d == NULL || d->kind != AST_FUNC_DEF || d->b == NULL || d->b->kind != AST_QUALIFIED_ID) continue;
        AstList *q = &d->b->list;
        /* the longest leading run of qualifier names that is a known namespace path */
        char buf[1024];
        buf[0] = '\0';
        int nsn = 0;
        for (int k = 0; k < q->count; k++) {
            const char *nm = q->items[k]->str1;
            size_t used = strlen(buf);
            if (nm == NULL || used + strlen(nm) + 3 >= sizeof buf) break;
            snprintf(buf + used, sizeof buf - used, "%s%s", used ? "::" : "", nm);
            if (!ns_path_known(paths, n, buf)) break;
            nsn = k + 1;
        }
        if (nsn == 0) continue;
        AstNode *ns_names[64];
        if (nsn > 64) nsn = 64;
        for (int k = 0; k < nsn; k++) ns_names[k] = q->items[k];
        AstNode *rest = NULL;
        if (nsn < q->count) {
            /* keep the class part as the qualifier chain */
            rest = ast_new(AST_QUALIFIED_ID, d->b->line);
            rest->file = d->b->file;
            for (int k = nsn; k < q->count; k++) ast_list_append(&rest->list, q->items[k]);
        }
        d->b = rest;
        if (rest == NULL) d->attr_flags |= ATTR_QUALIFIED_DEF;     /* sema: a declaration in `a` must match */
        for (int k = 0; k < d->list.count; k++)
            if (d->list.items[k] != NULL) strip_own_namespace(d->list.items[k]->type, ns_names, nsn);
        strip_own_namespace(d->type, ns_names, nsn);
        AstNode *inner = d;
        for (int k = nsn - 1; k >= 0; k--) {
            AstNode *w = ast_new(AST_NAMESPACE_DECL, d->line);
            w->file = d->file;
            w->str1 = strdup(ns_names[k]->str1);
            ast_list_append(&w->list, inner);
            inner = w;
        }
        decls->items[i] = inner;
    }
    for (int i = 0; i < n; i++) free(paths[i]);
    free(paths);
}

typedef struct { AstNode *node; char *key; char *qkey; } ExternKeyed;

static void collect_extern_keys(AstList *decls, const char *prefix, ExternKeyed **v, int *n, int *cap) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d == NULL) continue;
        if (d->kind == AST_NAMESPACE_DECL) {
            if (d->str1 == NULL) { collect_extern_keys(&d->list, prefix, v, n, cap); continue; }
            size_t len = strlen(prefix) + strlen(d->str1) + 3;
            char *np = malloc(len);
            snprintf(np, len, "%s%s__", prefix, d->str1);
            collect_extern_keys(&d->list, np, v, n, cap);
            free(np);
        } else if (d->kind == AST_VAR_DECL && d->str1 != NULL) {
            const char *pre = d->extern_decl ? "" : prefix;   /* an extern keeps its bare name */
            size_t len = strlen(pre) + strlen(d->str1) + 1;
            char *key = malloc(len);
            snprintf(key, len, "%s%s", pre, d->str1);
            if (*n == *cap) { *cap = *cap ? *cap * 2 : 64; *v = realloc(*v, sizeof(ExternKeyed) * (size_t)*cap); }
            size_t qlen = strlen(prefix) + strlen(d->str1) + 1;
            char *qkey = malloc(qlen);
            snprintf(qkey, qlen, "%s%s", prefix, d->str1);
            (*v)[*n].node = d;
            (*v)[*n].key = key;
            (*v)[*n].qkey = qkey;
            (*n)++;
        }
    }
}

static void drop_nodes(AstList *decls, AstNode **drop, int ndrop) {
    AstList out = ast_list_new();
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        int dropped = 0;
        for (int k = 0; k < ndrop && !dropped; k++) dropped = (drop[k] == d);
        if (dropped) continue;
        if (d != NULL && d->kind == AST_NAMESPACE_DECL) drop_nodes(&d->list, drop, ndrop);
        ast_list_append(&out, d);
    }
    *decls = out;
}

void merge_extern_names(AstList *decls) {
    ExternKeyed *v = NULL;
    int n = 0, cap = 0;
    collect_extern_keys(decls, "", &v, &n, &cap);
    AstNode **drop = NULL;
    int ndrop = 0;
    for (int i = 0; i < n; i++) {
        if (!v[i].node->extern_decl) continue;
        int drop_it = 0;
        for (int j = 0; j < n && !drop_it; j++) {
            if (j == i) continue;
            if (!v[j].node->extern_decl) {
                /* a definition: of the C name (`int c_side = 3;` at file
                 * scope), or of the very same namespace variable */
                if (strcmp(v[i].key, v[j].key) == 0 || strcmp(v[i].qkey, v[j].qkey) == 0) drop_it = 1;
            } else if (j < i && strcmp(v[i].key, v[j].key) == 0) {
                drop_it = 1;                    /* an earlier extern of it */
            }
        }
        if (drop_it) {
            drop = realloc(drop, sizeof(AstNode *) * (size_t)(ndrop + 1));
            drop[ndrop++] = v[i].node;
        }
    }
    if (ndrop > 0) drop_nodes(decls, drop, ndrop);
    for (int i = 0; i < n; i++) { free(v[i].key); free(v[i].qkey); }
    free(v);
    free(drop);
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

/* The typedefs of the program (`typedef Counter C;`), filled by
 * desugar_unnamed_objects (collect_scalar_names): a class is also known by
 * the names that are typedefs of it. */
static AstList g_scalar_typedefs;

static AstNode *class_named_depth(const ClassList *all, const char *name, int depth) {
    if (name == NULL || depth > 16) return NULL;
    for (int i = 0; i < all->count; i++)
        if (strcmp(all->items[i]->str1, name) == 0) return all->items[i];
    for (int i = 0; i < g_scalar_typedefs.count; i++) {
        const AstNode *td = g_scalar_typedefs.items[i];
        if (strcmp(td->str1, name) != 0) continue;
        const AstNode *t = td->type;
        while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
        if (t != NULL && t->kind == AST_IDENT && t->str1 != NULL && strcmp(t->str1, name) != 0)
            return class_named_depth(all, t->str1, depth + 1);
    }
    return NULL;
}

/* the class `name` stands for: a class's own name, or a typedef of it */
static AstNode *class_named(const ClassList *all, const char *name) {
    return class_named_depth(all, name, 0);
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

/* The class of a by-value member, or of the elements of a member ARRAY of
 * any number of dimensions (`Counter items[3]`, `Counter grid[2][2]`) -- the same members constructor
 * injection handles -- or NULL. Not a pointer or reference. */
static AstNode *member_value_class(const ClassList *all, const AstNode *m) {
    if (m == NULL || m->kind != AST_VAR_DECL || m->type == NULL) return NULL;
    const AstNode *t = m->type;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    while (t != NULL && t->kind == AST_ARRAY_TYPE) {
        t = t->a;
        while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    }
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
 *     declaration ahead of the loop would run once: reported as an error;
 *   - an element of an array of objects, `Enemy a[2] = { Enemy(1), {2} };`,
 *     is not a temporary either: C++17 builds it in place, so no object is
 *     declared. The element stays where it is as an AST_DIRECT_INIT with
 *     its class as the type (a `{2}` becomes one too), and the constructor
 *     injection in lower.c calls the right constructor on `a[0]`, `a[1]`
 *     ... (the elements left out are default-constructed). Without this
 *     the temporaries were built, copied into the array and destroyed
 *     with it at scope exit: the array's own elements never had a
 *     constructor run, and every destructor ran twice.
 *
 * One known difference from C++: in `a && f(Enemy(1))` the object is
 * built even when `a` is false.
 *
 * A typedef or enum name used the same way is a cast (`Fixed(3)` is
 * `(Fixed)3`) and is rewritten as one.
 */
static int g_obj_counter = 0;
static int g_obj_errors = 0;
static int g_obj_in_class = 0; /* desugar_objects_decls is inside a class body */

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
static AstNode *array_element_class_of_type(const AstNode *t, const ClassList *all);
static void keep_array_elements(AstNode *list, int depth, int ndims, AstNode *cls,
                                const ClassList *all, AstList *prefix);

static void lift_objects_expr(AstNode **slot, const ClassList *all, AstList *prefix) {
    AstNode *n = *slot;
    if (n == NULL) return;
    if (n->kind == AST_NEW && n->a != NULL && n->b != NULL && n->b->kind == AST_INIT_LIST) {
        /* `new T[n]{a, {b, c}, T(d)}`: the elements of the brace list are built in
         * place, as for a local array of objects (keep_array_elements), and move to
         * the node's own list; the now empty list in `b` marks it as list-initialized */
        lift_objects_expr(&n->a, all, prefix);
        AstNode *cls = array_element_class_of_type(n->type, all);
        if (cls != NULL) {
            keep_array_elements(n->b, 0, 1, cls, all, prefix);
        } else {
            for (int i = 0; i < n->b->list.count; i++) {
                if (n->b->list.items[i] != NULL && n->b->list.items[i]->kind == AST_INIT_LIST) continue;
                lift_objects_expr(&n->b->list.items[i], all, prefix);
            }
        }
        n->list = n->b->list;
        n->b->list = ast_list_new();
        return;
    }
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

/* The class whose objects make up the array type `t` (any number of
 * dimensions, `Enemy grid[2][3]`), or NULL; *ndims gets the dimension count. */
static AstNode *array_object_class(const AstNode *t, const ClassList *all, int *ndims) {
    int dims = 0;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    while (t != NULL && t->kind == AST_ARRAY_TYPE) {
        dims++;
        t = t->a;
        while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    }
    if (dims == 0 || t == NULL || t->kind != AST_IDENT) return NULL;
    *ndims = dims;
    return class_named(all, t->str1);
}

static AstNode *array_element_class_of_type(const AstNode *t, const ClassList *all) {
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    if (t == NULL || t->kind != AST_IDENT || t->str1 == NULL) return NULL;
    return class_named(all, t->str1);
}

/* The elements of the brace list `list` (one nesting level per array
 * dimension) that are objects of `cls`: each `Enemy(args)` and, for a
 * class with a constructor, each `{args}`, becomes an in-place
 * AST_DIRECT_INIT of type `cls` (see the doc comment above); the
 * arguments themselves, and any other element, are searched for unnamed
 * objects as usual. */
static void keep_array_elements(AstNode *list, int depth, int ndims, AstNode *cls,
                                const ClassList *all, AstList *prefix) {
    for (int i = 0; i < list->list.count; i++) {
        AstNode **slot = &list->list.items[i];
        AstNode *item = *slot;
        if (item == NULL) continue;
        if (depth + 1 < ndims) {
            if (item->kind == AST_INIT_LIST) keep_array_elements(item, depth + 1, ndims, cls, all, prefix);
            else lift_objects_expr(slot, all, prefix);
            continue;
        }
        int own = is_unnamed_object(item) && class_named(all, item->str1) == cls;
        int braced = item->kind == AST_INIT_LIST && class_declares_ctor(cls);
        if (!own && !braced) {
            lift_objects_expr(slot, all, prefix);
            /* any other value (`a[0] = other`, or `5` for a converting constructor) copy-
             * initializes the element: `Counter(value)`, again in place */
            if (item->kind != AST_INIT_LIST && item->kind != AST_DESIG_INIT && class_declares_ctor(cls)) {
                AstNode *init = ast_new(AST_DIRECT_INIT, item->line);
                init->file = item->file;
                init->type = ast_ident(cls->str1, item->line);
                init->ival = 1; /* a copy: lower.c assigns it where the class has no copy constructor */
                ast_list_append(&init->list, *slot);
                *slot = init;
            }
            continue;
        }
        for (int k = 0; k < item->list.count; k++) lift_objects_expr(&item->list.items[k], all, prefix);
        AstNode *init = item;
        if (braced) {
            init = ast_new(AST_DIRECT_INIT, item->line);
            init->file = item->file;
            init->list = item->list;
        } else {
            free(item->str1);
            item->str1 = NULL;
        }
        init->type = ast_ident(cls->str1, item->line);
        *slot = init;
    }
}

/* `T x = T(args);` -> `T x(args);` */
static void elide_copy_from_object(AstNode *decl, const ClassList *all, AstList *prefix) {
    if (decl->kind != AST_VAR_DECL || !is_unnamed_object(decl->a)) return;
    const AstNode *t = decl->type;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    if (t == NULL || t->kind != AST_IDENT) return;
    AstNode *cls = class_named(all, decl->a->str1);
    if (cls == NULL || class_named(all, t->str1) != cls) return; /* `C x = C(1);` and `Counter x = C(1);` */
    AstNode *obj = decl->a;
    for (int i = 0; i < obj->list.count; i++) lift_objects_expr(&obj->list.items[i], all, prefix);
    decl->a = object_initializer(obj);
}

/* C++11 list-initialization of a variable: `T x{a, b};` and `T x = {a, b};`
 * (the parser gives both an AST_INIT_LIST).
 *   - a class that declares a constructor: the same as `T x(a, b);` (an
 *     AST_DIRECT_INIT; `{}` is `T x;`)
 *   - a scalar (a primitive type or a pointer): `int k = 3;` (`{}` is 0)
 *   - anything else (an aggregate, an array): left as a C brace list. */
/* Enum names and typedefs, for `Color c{RED}` / `score_t s{4}`: both are
 * scalars too. Filled by desugar_unnamed_objects before the rewrite runs. */
static AstList g_scalar_names_enums;

static void collect_scalar_names(const AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_ENUM_DECL && n->str1 != NULL) ast_list_append(&g_scalar_names_enums, n);
        else if (n->kind == AST_TYPEDEF_DECL && n->str1 != NULL) ast_list_append(&g_scalar_typedefs, n);
        else if (n->kind == AST_NAMESPACE_DECL || n->kind == AST_CLASS_DECL) collect_scalar_names(&n->list);
    }
}

static int is_scalar_decl_type_depth(const AstNode *t, int depth) {
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    if (t == NULL || depth > 16) return 0;
    if (t->kind == AST_POINTER_TYPE) return 1;
    if (t->kind != AST_IDENT || t->str1 == NULL) return 0;
    static const char *const names[] = { "int", "float", "bool", "char", "short", "long", "double", NULL };
    for (int i = 0; names[i] != NULL; i++)
        if (strcmp(t->str1, names[i]) == 0) return 1;
    for (int i = 0; i < g_scalar_names_enums.count; i++)
        if (strcmp(g_scalar_names_enums.items[i]->str1, t->str1) == 0) return 1;
    for (int i = 0; i < g_scalar_typedefs.count; i++) {
        const AstNode *td = g_scalar_typedefs.items[i];
        if (strcmp(td->str1, t->str1) == 0) return is_scalar_decl_type_depth(td->type, depth + 1);
    }
    return 0;
}

static int is_scalar_decl_type(const AstNode *t) { return is_scalar_decl_type_depth(t, 0); }

static void rewrite_list_init(AstNode *decl, const ClassList *all) {
    if (decl->kind != AST_VAR_DECL || decl->a == NULL || decl->a->kind != AST_INIT_LIST) return;
    AstNode *list = decl->a;
    const AstNode *t = decl->type;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    AstNode *cls = (t != NULL && t->kind == AST_IDENT) ? class_named(all, t->str1) : NULL;
    int scalar = cls == NULL && is_scalar_decl_type(decl->type);
    if (cls != NULL && !class_declares_ctor(cls)) return; /* an aggregate */
    if (cls == NULL && !scalar) return;
    for (int i = 0; i < list->list.count; i++) {
        const AstNode *item = list->list.items[i];
        if (item != NULL && (item->kind == AST_INIT_LIST || item->kind == AST_DESIG_INIT)) {
            obj_error(decl, "a nested braced or designated initializer inside `{...}` for a "
                            "variable that is not an aggregate is not supported: pass plain values");
            return;
        }
    }
    if (scalar) {
        if (list->list.count > 1) {
            obj_error(decl, "a scalar variable can be initialized from at most one value in braces");
            return;
        }
        if (list->list.count == 1) {
            decl->a = list->list.items[0];
        } else {
            AstNode *zero = ast_new(AST_INT_LIT, decl->line);
            zero->file = decl->file;
            zero->ival = 0;
            decl->a = zero;
        }
        return;
    }
    if (list->list.count == 0) { decl->a = NULL; return; } /* `T x{};` is `T x;` */
    AstNode *init = ast_new(AST_DIRECT_INIT, list->line);
    init->file = list->file;
    init->list = list->list;
    decl->a = init;
}

static void lift_objects_decl(AstNode *n, const ClassList *all, AstList *prefix) {
    if (n->kind == AST_VAR_DECL_GROUP) {
        for (int i = 0; i < n->list.count; i++) lift_objects_decl(n->list.items[i], all, prefix);
        return;
    }
    rewrite_list_init(n, all);
    elide_copy_from_object(n, all, prefix);
    int ndims = 0;
    AstNode *cls = (n->kind == AST_VAR_DECL && n->a != NULL && n->a->kind == AST_INIT_LIST)
                       ? array_object_class(n->type, all, &ndims) : NULL;
    if (cls != NULL) keep_array_elements(n->a, 0, ndims, cls, all, prefix);
    else lift_objects_expr(&n->a, all, prefix);
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

static AstNode *g_obj_class = NULL; /* the class whose body desugar_objects_decls is in */

/* `Holder() : m{ {1}, Counter(2) } {}` -- a constructor's initializer for an
 * array member of class type: the elements become in-place constructor calls,
 * exactly as for `Counter m[2] = {...}` (the member's own list is handled in
 * desugar_objects_decls). Any other entry is left alone. */
static void desugar_member_init_arrays(AstNode *func, const ClassList *all) {
    const AstNode *owner = g_obj_class;
    if (func->b != NULL) { /* defined out of line: `Holder::Holder() : ...` */
        const AstNode *q = func->b;
        const char *name = q->kind == AST_IDENT ? q->str1
                         : (q->list.count > 0 ? q->list.items[q->list.count - 1]->str1 : NULL);
        owner = class_named(all, name);
    }
    if (owner == NULL) return;
    for (int i = 0; i < func->c->list.count; i++) {
        AstNode *entry = func->c->list.items[i];
        const AstNode *field = NULL;
        for (int j = 0; j < owner->list.count; j++) {
            const AstNode *m = owner->list.items[j];
            if (m != NULL && m->kind == AST_VAR_DECL && m->str1 != NULL && strcmp(m->str1, entry->str1) == 0) field = m;
        }
        int ndims = 0;
        AstNode *cls = field != NULL ? array_object_class(field->type, all, &ndims) : NULL;
        if (cls == NULL || !class_declares_ctor(cls)) continue;
        AstList none = ast_list_new();
        keep_array_elements(entry, 0, ndims, cls, all, &none);
        if (none.count > 0)
            obj_error(entry, "an unnamed object nested in an array member's initializer is not supported: "
                             "write the elements as `Counter(args)` or `{args}`");
    }
}

static void desugar_objects_decls(AstList *decls, const ClassList *all) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n == NULL) continue;
        int errors_before = g_obj_errors;
        if (n->kind == AST_NAMESPACE_DECL || n->kind == AST_CLASS_DECL) {
            int was_in_class = g_obj_in_class;
            AstNode *was_class = g_obj_class;
            if (n->kind == AST_CLASS_DECL) { g_obj_in_class = 1; g_obj_class = n; }
            desugar_objects_decls(&n->list, all);
            g_obj_in_class = was_in_class;
            g_obj_class = was_class;
        } else if (n->kind == AST_FUNC_DEF) {
            g_obj_counter = 0;
            if (n->c != NULL) desugar_member_init_arrays(n, all);
            desugar_objects_slot(&n->a, all);
        } else if (n->kind == AST_VAR_DECL || n->kind == AST_VAR_DECL_GROUP) {
            /* file scope: only the `T g = T(args);` form has anywhere to go */
            AstList none = ast_list_new();
            if (!g_obj_in_class) {
                if (n->kind == AST_VAR_DECL_GROUP)
                    for (int k = 0; k < n->list.count; k++) rewrite_list_init(n->list.items[k], all);
                else rewrite_list_init(n, all);
            }
            if (n->kind == AST_VAR_DECL) {
                elide_copy_from_object(n, all, &none);
                int ndims = 0;
                /* a global array of objects: built in place, at the top of main() (a
                 * class member's `= {...}` is not this: it stays as it was) */
                AstNode *cls = (n->a != NULL && n->a->kind == AST_INIT_LIST)
                                   ? array_object_class(n->type, all, &ndims) : NULL;
                if (cls != NULL && g_obj_in_class) {
                    /* `Counter m[2] = { {1}, Counter(2) };` as a member: the elements
                     * become in-place constructor calls, run by every constructor that
                     * does not name the member (apply_default_member_initializers) */
                    if (class_declares_ctor(cls)) keep_array_elements(n->a, 0, ndims, cls, all, &none);
                } else if (cls != NULL) {
                    keep_array_elements(n->a, 0, ndims, cls, all, &none);
                }
                if (none.count > 0)
                    obj_error(n, g_obj_in_class
                                 ? "an unnamed object nested in an array member's initializer is not supported: "
                                   "write the elements as `Counter(args)` or `{args}`"
                                 : "an unnamed object nested in a file-scope initializer is not supported: "
                                   "declare it as a variable");
            }
        }
        if (g_obj_errors == errors_before) report_leftover_objects(n);
    }
}

/* A typedef used as a base class: `typedef Base B; class D : public B {...};`
 * and `D() : B(3) {}`. The base is renamed to the class it stands for before
 * sema (which looks bases up by their class name), in the class itself, in
 * the initializer lists of its constructors (in-class or out of line). */
static void canonicalize_init_list(AstNode *ctor, const AstNode *base, const AstNode *cls, const ClassList *all) {
    if (ctor->c == NULL) return;
    for (int i = 0; i < ctor->c->list.count; i++) {
        AstNode *e = ctor->c->list.items[i];
        if (e == NULL || e->str1 == NULL || strcmp(e->str1, base->str1) == 0) continue;
        if (class_named(all, e->str1) != base) continue;
        int is_member = 0;
        for (int k = 0; k < cls->list.count; k++)
            if (cls->list.items[k] != NULL && cls->list.items[k]->kind == AST_VAR_DECL &&
                strcmp(cls->list.items[k]->str1, e->str1) == 0) is_member = 1;
        if (is_member) continue;
        e->str1 = strdup(base->str1);
    }
}

static void canonicalize_out_of_line_ctors(AstList *decls, const AstNode *cls, const AstNode *base, const ClassList *all) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_NAMESPACE_DECL) { canonicalize_out_of_line_ctors(&n->list, cls, base, all); continue; }
        if (n->kind != AST_FUNC_DEF || n->b == NULL || n->str1 == NULL) continue;
        if (strcmp(n->str1, cls->str1) != 0) continue;
        canonicalize_init_list(n, base, cls, all);
    }
}

static void canonicalize_typedef_bases(AstList *decls, const ClassList *all) {
    for (int i = 0; i < all->count; i++) {
        AstNode *c = all->items[i];
        if (c->str2 == NULL) continue;
        int exact = 0;
        for (int k = 0; k < all->count; k++)
            if (strcmp(all->items[k]->str1, c->str2) == 0) exact = 1;
        if (exact) continue;
        AstNode *base = class_named(all, c->str2);
        if (base == NULL) continue;   /* sema reports an unknown base */
        c->str2 = strdup(base->str1);
        for (int j = 0; j < c->list.count; j++) {
            AstNode *m = c->list.items[j];
            if (is_func(m) && strcmp(m->str1, c->str1) == 0) canonicalize_init_list(m, base, c, all);
        }
        canonicalize_out_of_line_ctors(decls, c, base, all);
    }
}

int desugar_unnamed_objects(AstList *decls) {
    g_scalar_names_enums = ast_list_new();
    g_scalar_typedefs = ast_list_new();
    collect_scalar_names(decls);
    ClassList all = { NULL, 0, 0 };
    collect_classes(decls, &all);
    canonicalize_typedef_bases(decls, &all);
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

/* ---- in-class default member initializers --------------------------------
 *
 *     class Player { int mLives = 3; bool mWantsFire = false; ... };
 *
 * These parsed (a member is a declaration like any other, initializer
 * and all) and were then ignored: the generated struct has no place for
 * an initial value, so mLives started as whatever was in memory. C++
 * says each constructor initializes such a member with that value unless
 * its own initializer list mentions the member. That is what this does,
 * before sema: the value moves into an assignment at the top of every
 * constructor of the class (in-class or defined outside it), and a class
 * with no constructor at all gets one.
 *
 * The initializer is an expression assigned with `=`. A brace list on an array
 * of objects whose class has a constructor (`Counter m[2] = { {1}, Counter(2) };`)
 * becomes an entry of the constructor's own initializer list instead (the
 * elements are built in place, see add_array_member_init); any other brace
 * list (a plain C array member's `= {1, 2, 3}`) is left alone.
 */
static int plain_scalar_array_list(const AstNode *m);

static int ctor_initializes(const AstNode *ctor, const char *member) {
    if (ctor->c == NULL) return 0;
    for (int i = 0; i < ctor->c->list.count; i++)
        if (strcmp(ctor->c->list.items[i]->str1, member) == 0) return 1;
    return 0;
}

/* An array member of class type with a brace list (`Counter m[2] = {{1}, {2}};`):
 * not an assignment but a constructor-initializer entry, `: m{...}`, which
 * lower.c turns into one constructor call per element (phase 8-pre / 7). */
static void add_array_member_init(AstNode *ctor, const AstNode *f) {
    AstNode *entry = ast_new(AST_MEMBER_INIT, f->line);
    entry->file = f->file;
    entry->str1 = strdup(f->str1);
    AstNode *items = ast_clone_expr(f->a);
    entry->list = items->list;
    if (ctor->c == NULL) {
        ctor->c = ast_new(AST_MEMBER_INIT_LIST, f->line);
        ctor->c->file = f->file;
    }
    ast_list_append(&ctor->c->list, entry);
}

static void prepend_member_defaults(AstNode *ctor, const AstList *fields) {
    if (ctor->a == NULL || ctor->a->kind != AST_BLOCK) return;
    AstList body = ast_list_new();
    for (int i = 0; i < fields->count; i++) {
        AstNode *f = fields->items[i];
        if (ctor_initializes(ctor, f->str1)) continue;
        if (f->a->kind == AST_INIT_LIST && plain_scalar_array_list(f)) {
            const AstNode *t = f->type;
            while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
            const AstNode *et = t->a;
            while (et != NULL && et->kind == AST_CONST_TYPE) et = et->a;
            for (int k = 0; k < t->ival; k++) {
                AstNode *field = ast_new(AST_MEMBER, f->line);
                field->file = f->file;
                field->str1 = strdup("->");
                field->str2 = strdup(f->str1);
                field->a = ast_new(AST_THIS, f->line);
                AstNode *sub = ast_new(AST_SUBSCRIPT, f->line);
                sub->file = f->file;
                sub->a = field;
                sub->b = ast_new(AST_INT_LIT, f->line);
                sub->b->ival = k;
                AstNode *value;
                if (k < f->a->list.count) value = ast_clone_expr(f->a->list.items[k]);
                else if (et->kind == AST_POINTER_TYPE) value = ast_new(AST_NULL_LIT, f->line);
                else if (et->str1 != NULL && (strcmp(et->str1, "float") == 0 || strcmp(et->str1, "double") == 0)) {
                    value = ast_new(AST_FLOAT_LIT, f->line);
                    value->fval = 0.0;
                } else {
                    value = ast_new(AST_INT_LIT, f->line);
                    value->ival = 0;
                }
                AstNode *assign = ast_new(AST_ASSIGN, f->line);
                assign->file = f->file;
                assign->str1 = strdup("=");
                assign->a = sub;
                assign->b = value;
                AstNode *st = ast_new(AST_EXPR_STMT, f->line);
                st->file = f->file;
                st->a = assign;
                ast_list_append(&body, st);
            }
            continue;
        }
        if (f->a->kind == AST_INIT_LIST) { add_array_member_init(ctor, f); continue; }
        AstNode *assign = ast_new(AST_ASSIGN, f->line);
        assign->file = f->file;
        assign->str1 = strdup("=");
        /* `this->member`, spelled out: a constructor parameter may have
         * the member's name */
        AstNode *field = ast_new(AST_MEMBER, f->line);
        field->file = f->file;
        field->str1 = strdup("->");
        field->str2 = strdup(f->str1);
        field->a = ast_new(AST_THIS, f->line);
        assign->a = field;
        assign->b = ast_clone_expr(f->a);
        AstNode *st = ast_new(AST_EXPR_STMT, f->line);
        st->file = f->file;
        st->a = assign;
        ast_list_append(&body, st);
    }
    for (int i = 0; i < ctor->a->list.count; i++) ast_list_append(&body, ctor->a->list.items[i]);
    ctor->a->list = body;
}

/* Constructors of `cls` defined OUTSIDE the class body: `Player::Player() {...}`. */
static void default_inits_out_of_line(AstList *decls, const AstNode *cls, const AstList *fields) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_NAMESPACE_DECL) { default_inits_out_of_line(&n->list, cls, fields); continue; }
        if (n->kind != AST_FUNC_DEF || n->b == NULL || n->str1 == NULL) continue;
        if (strcmp(n->str1, cls->str1) != 0) continue;
        const AstNode *q = n->b;   /* the qualifier: its last name is the class */
        const char *owner = NULL;
        if (q->kind == AST_IDENT) owner = q->str1;
        else if (q->list.count > 0) owner = q->list.items[q->list.count - 1]->str1;
        if (owner != NULL && strcmp(owner, cls->str1) == 0) prepend_member_defaults(n, fields);
    }
}

/* `int a[3] = {1, 2, 3};` / `int a[3]{1, 2};` as a member default: a one-dimensional array of a
 * scalar type whose list holds plain values. Assigned element by element in each constructor (the
 * rest of the elements zero, as in aggregate initialization). */
static int plain_scalar_array_list(const AstNode *m) {
    const AstNode *t = m->type;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    if (t == NULL || t->kind != AST_ARRAY_TYPE || t->a == NULL || m->a == NULL || m->a->kind != AST_INIT_LIST) return 0;
    const AstNode *et = t->a;
    while (et != NULL && et->kind == AST_CONST_TYPE) et = et->a;
    if (et == NULL || (et->kind != AST_IDENT && et->kind != AST_POINTER_TYPE)) return 0;
    if (et->kind == AST_IDENT) {
        static const char *const ok[] = { "int", "float", "char", "bool", "double", "short", "long", "unsigned", NULL };
        int found = 0;
        for (int i = 0; ok[i] != NULL; i++) if (et->str1 != NULL && strcmp(et->str1, ok[i]) == 0) found = 1;
        if (!found) return 0;
    }
    for (int i = 0; i < m->a->list.count; i++)
        if (m->a->list.items[i] == NULL || m->a->list.items[i]->kind == AST_INIT_LIST ||
            m->a->list.items[i]->kind == AST_DESIG_INIT) return 0;
    return t->ival >= m->a->list.count;
}

void apply_default_member_initializers(AstList *decls) {
    ClassList all = { NULL, 0, 0 };
    collect_classes(decls, &all);
    for (int i = 0; i < all.count; i++) {
        AstNode *c = all.items[i];
        AstList fields = ast_list_new();
        for (int j = 0; j < c->list.count; j++) {
            AstNode *m = c->list.items[j];
            if (m == NULL || m->kind != AST_VAR_DECL || m->a == NULL) continue;
            if (m->a->kind == AST_INIT_LIST) {
                /* only an array of objects whose class has a constructor: the rest of the
                 * brace lists are C aggregates, left alone */
                int nd = 0;
                AstNode *ec = array_object_class(m->type, &all, &nd);
                if (ec != NULL) {
                    if (!class_declares_ctor(ec)) continue;
                } else {
                    /* `int w{5}`, `P n{1, 2}`, `Counter c{3}`: one member, the list is its `: m{...}` entry;
                     * a plain C array member keeps its list */
                    const AstNode *mt = m->type;
                    while (mt != NULL && mt->kind == AST_CONST_TYPE) mt = mt->a;
                    if (mt == NULL) continue;
                    if (mt->kind == AST_ARRAY_TYPE && !plain_scalar_array_list(m)) continue;
                }
            } else if (m->a->kind == AST_DIRECT_INIT) {
                continue;
            }
            ast_list_append(&fields, m);
        }
        if (fields.count == 0) continue;
        int declared = 0;
        for (int j = 0; j < c->list.count; j++) {
            AstNode *m = c->list.items[j];
            if (!is_func(m) || strcmp(m->str1, c->str1) != 0) continue;
            declared = 1;
            if (m->kind == AST_FUNC_DEF) prepend_member_defaults(m, &fields);
        }
        default_inits_out_of_line(decls, c, &fields);
        if (!declared) {
            AstNode *pub = ast_new(AST_ACCESS_SPEC, c->line);
            pub->access = ACC_PUBLIC;
            ast_list_append(&c->list, pub);
            AstNode *ctor = make_implicit_member(c, 0);
            prepend_member_defaults(ctor, &fields);
            ast_list_append(&c->list, ctor);
        }
        for (int j = 0; j < fields.count; j++) fields.items[j]->a = NULL;
    }
    free(all.items);
}
