#ifndef AST_H
#define AST_H

/*
 * Deliberately un-fancy AST: one tagged struct with a handful of generic
 * child slots plus a generic child list, rather than a distinct C struct
 * per node kind. That's a readability/maintainability trade-off -- for a
 * grammar this size it keeps parser.y actions short (no per-kind
 * constructor boilerplate), at the cost of field names like a/b/c/d whose
 * meaning depends on `kind`. Comments below spell out the convention per
 * kind. If/when this grows past ~30 node kinds, switching to a tagged
 * union of real structs (or generating it) starts paying for itself.
 */

typedef enum {
    AST_PROGRAM,
    AST_NAMESPACE_DECL,   /* str1=name, list=members */
    AST_CLASS_DECL,       /* str1=name, str2=base name or NULL, list=members */
    AST_ACCESS_SPEC,      /* access=the new default access for what follows */
    AST_VAR_DECL,         /* str1=name, type=declared type, a=initializer or NULL */
    AST_TYPEDEF_DECL,     /* str1=new name, type=underlying type */
    AST_FUNC_DECL,        /* str1=name, type=return type (NULL for ctor/dtor),
                              list=params, access, a=NULL (no body) */
    AST_FUNC_DEF,         /* same as AST_FUNC_DECL but a=body (AST_BLOCK) */
    AST_PARAM,            /* str1=name, type=param type */
    AST_BLOCK,            /* list=statements */
    AST_IF,               /* a=cond, b=then-stmt, c=else-stmt or NULL */
    AST_WHILE,            /* a=cond, b=body */
    AST_FOR,              /* a=init-stmt or NULL, b=cond or NULL, c=step-expr or NULL, d=body */
    AST_RETURN,           /* a=expr or NULL */
    AST_EXPR_STMT,        /* a=expr */
    AST_BINOP,            /* str1=operator text, a=lhs, b=rhs */
    AST_UNOP,             /* str1=operator text, a=operand */
    AST_ASSIGN,           /* str1=operator text ("=","+=",...), a=lhs, b=rhs */
    AST_CALL,             /* a=callee, list=args */
    AST_MEMBER,           /* str1=".": or "->", str2=member name, a=object */
    AST_SUBSCRIPT,        /* a=array, b=index */
    AST_IDENT,            /* str1=name */
    AST_QUALIFIED_ID,     /* list=AST_IDENT parts, e.g. [v32, Timer] */
    AST_INT_LIT,          /* ival */
    AST_FLOAT_LIT,        /* fval */
    AST_STRING_LIT,       /* str1=text (raw, unescaped as lexed) */
    AST_CHAR_LIT,         /* ival=char code */
    AST_BOOL_LIT,         /* ival=0/1 */
    AST_THIS,
    AST_NEW,              /* type=type being allocated */
    AST_DELETE            /* a=expr being deleted */
} AstKind;

typedef enum { ACC_PUBLIC, ACC_PRIVATE, ACC_PROTECTED } AccessSpec;

typedef struct AstNode AstNode;

typedef struct AstList {
    AstNode **items;
    int count;
    int capacity;
} AstList;

struct AstNode {
    AstKind kind;
    int line;

    char *str1;
    char *str2;
    AstNode *type;      /* used for declared/return/param types */
    AccessSpec access;

    int ival;
    double fval;

    AstNode *a, *b, *c, *d;
    AstList list;
};

AstList ast_list_new(void);
void ast_list_append(AstList *list, AstNode *node);

AstNode *ast_new(AstKind kind, int line);
AstNode *ast_ident(const char *name, int line);

void ast_dump(const AstNode *node, int indent);

#endif /* AST_H */
