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

AstNode *ast_wrap_array(AstNode *inner, int length, int line) {
    AstNode *n = ast_new(AST_ARRAY_TYPE, line);
    n->a = inner;
    n->ival = length;
    return n;
}

static const char *kind_name(AstKind k) {
    switch (k) {
        case AST_PROGRAM: return "Program";
        case AST_NAMESPACE_DECL: return "NamespaceDecl";
        case AST_CLASS_DECL: return "ClassDecl";
        case AST_ACCESS_SPEC: return "AccessSpec";
        case AST_VAR_DECL: return "VarDecl";
        case AST_TYPEDEF_DECL: return "TypedefDecl";
        case AST_FUNC_DECL: return "FuncDecl";
        case AST_FUNC_DEF: return "FuncDef";
        case AST_PARAM: return "Param";
        case AST_BLOCK: return "Block";
        case AST_IF: return "If";
        case AST_WHILE: return "While";
        case AST_FOR: return "For";
        case AST_RETURN: return "Return";
        case AST_BREAK: return "Break";
        case AST_CONTINUE: return "Continue";
        case AST_EXPR_STMT: return "ExprStmt";
        case AST_BINOP: return "BinOp";
        case AST_UNOP: return "UnOp";
        case AST_ASSIGN: return "Assign";
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
        case AST_THIS: return "This";
        case AST_NEW: return "New";
        case AST_DELETE: return "Delete";
        case AST_POINTER_TYPE: return "PointerType";
        case AST_REFERENCE_TYPE: return "ReferenceType";
        case AST_ARRAY_TYPE: return "ArrayType";
        case AST_INIT_LIST: return "InitList";
        case AST_CAST: return "Cast";
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
