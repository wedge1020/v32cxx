#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

AstNode *ast_new(AstKind kind, int line) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->kind = kind;
    n->line = line;
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
    }
    return result;
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
