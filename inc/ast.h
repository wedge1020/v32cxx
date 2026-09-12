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
                              list=params, access, a=NULL (no body).
                              ival=virtual-ness: 1 if `virtual` was written
                              on THIS declaration. sema.c's vtable builder
                              may ALSO set this to 1 after parsing, on a
                              method that silently overrides an inherited
                              virtual slot without repeating the keyword
                              (matching real C++) -- so post-sema, ival
                              means "is this virtual", not just "was
                              'virtual' literally written here". Always 0
                              on an out-of-line definition itself (the
                              grammar doesn't accept `virtual` there,
                              matching real C++ -- it only ever belongs on
                              the in-class declaration).
                              b=NULL normally; only ever non-NULL on an
                              AST_FUNC_DEF built by out_of_line_def in
                              parser.y, where b=AST_QUALIFIED_ID holding
                              the Class:: (or Namespace::Class::) qualifier
                              chain the definition was written against. */
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
    AST_DELETE,           /* a=expr being deleted */
    AST_POINTER_TYPE,     /* a=pointee type -- represents "T *" */
    AST_REFERENCE_TYPE    /* a=referent type -- represents "T &" */
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

    /*
     * Opaque annotation slot for later compiler passes (semantic
     * analysis, lowering, ...) to attach computed, pass-specific
     * information without growing this struct per feature. NULL until a
     * pass populates it; what it points to depends on both node->kind and
     * which pass has run. Currently used by sema.c:
     *   - AST_CLASS_DECL  -> ClassLayout*   (see sema.h)
     *   - AST_FUNC_DECL/AST_FUNC_DEF -> FuncSemaInfo* (see sema.h)
     * ast_new() zero-initializes this via calloc, so it's safely NULL
     * on every node until something sets it.
     */
    void *sema_info;
};

AstList ast_list_new(void);
void ast_list_append(AstList *list, AstNode *node);

AstNode *ast_new(AstKind kind, int line);
AstNode *ast_ident(const char *name, int line);

/* Wrap `inner` (a type node) as "inner *" / "inner &". Used wherever a
 * declarator's pointer_opt (0=none, 1=*, 2=&) needs to be attached to the
 * type it modifies, rather than silently discarded -- see var_decl, param,
 * and typedef_decl in parser.y.
 *
 * LIMITATION: only a single level of indirection is modeled (`Type *p` or
 * `Type &r`), not `Type **pp` or combinations like `Type *&ref`
 * (reference-to-pointer, which is legal C++). Extending pointer_opt to a
 * proper chain is the natural next step if/when you need it -- change it
 * from a single int to a small list of modifier tags and call
 * ast_wrap_pointer/ast_wrap_reference once per entry, innermost first. */
AstNode *ast_wrap_pointer(AstNode *inner, int line);
AstNode *ast_wrap_reference(AstNode *inner, int line);

void ast_dump(const AstNode *node, int indent);

#endif /* AST_H */
