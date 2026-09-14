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
    AST_CLASS_DECL,       /* str1=name, str2=base name or NULL, list=members.
                              access=the inheritance access-specifier
                              (public/private/protected on `: public Base`
                              etc.) -- meaningless when str2 is NULL (no
                              base at all); sema.c reads this when
                              computing effective access of inherited
                              members. */
    AST_ACCESS_SPEC,      /* access=the new default access for what follows */
    AST_VAR_DECL,         /* str1=name, type=declared type, a=initializer or NULL.
                              access: meaningful only once sema.c has run and
                              this is a class member -- see compute_layout()
                              in sema.c, which stamps each member's access
                              level here as it walks the class body tracking
                              AST_ACCESS_SPEC markers (defaulting to
                              ACC_PRIVATE per `class`'s C++ default when no
                              marker precedes it). Meaningless/unset on a
                              free (non-member) variable declaration. */
    AST_TYPEDEF_DECL,     /* str1=new name, type=underlying type */
    AST_FUNC_DECL,        /* str1=name, type=return type (NULL for ctor/dtor),
                              list=params, a=NULL (no body).
                              str1 for an operator overload is literally
                              "operator+", "operator==", "operator[]", etc.
                              (see operator_symbol in parser.y for the
                              full supported list) -- an ordinary string,
                              handled like any other function name
                              everywhere except sema.c's mangle(), which
                              maps it to a C-identifier-safe fragment
                              (op_add, op_eq, ...) the same way it already
                              maps a destructor's "~Foo" to "dtor".
                              access: same meaning and same sema.c-stamped
                              timing as on AST_VAR_DECL above -- meaningless
                              until sema.c has run, and only meaningful at
                              all for a class member, not a free function.
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
    AST_BREAK,            /* no fields -- a leaf statement, `break;`.
                              sema.c rejects one appearing outside a loop
                              (real C++/C requires this too); lower.c's
                              phase 9 destroys whatever's live inside the
                              loop being exited (but nothing outside it)
                              before it executes -- see that phase's own
                              doc comment in lower.h for the full
                              reasoning. */
    AST_CONTINUE,         /* no fields -- `continue;`, otherwise identical
                              treatment to AST_BREAK in both sema.c and
                              lower.c (the two differ only in what they
                              compile TO -- codegen.c just emits each as
                              the literal C keyword -- not in how either
                              is validated or what gets destroyed before
                              one executes) */
    AST_EXPR_STMT,        /* a=expr */
    AST_BINOP,            /* str1=operator text, a=lhs, b=rhs */
    AST_UNOP,             /* str1=operator text, a=operand */
    AST_ASSIGN,           /* str1=operator text ("=","+=",...), a=lhs, b=rhs */
    AST_CALL,             /* a=callee, list=args.
                              sema_info: NULL until sema.c's overload-
                              resolution pass runs. If it resolved this
                              call to exactly one candidate (whether
                              because there was only one function by that
                              name, or because argument types picked one
                              out among several), sema_info becomes a
                              CallResolution* (see sema.h) pointing at it.
                              Staying NULL after the pass runs is NOT
                              necessarily an error: it also covers "no
                              function by this name was found at all" and
                              "genuinely overloaded, but an argument's
                              type couldn't be confidently determined" --
                              both silently skipped, as opposed to "no
                              candidate's signature matched" or
                              "more than one candidate matched", which
                              DO report an error (see resolve_call() in
                              sema.c for exactly which case is which). */
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
    AST_NEW,              /* type=type being allocated, list=constructor
                              arguments (may be empty -- `new T` and
                              `new T()` both produce an empty list; this
                              project doesn't distinguish the two, unlike
                              real C++'s default- vs value-initialization
                              subtlety).
                              a=array size expression for `new T[N]`
                              (NULL for the ordinary, single-object
                              form) -- unlike a stack array's own
                              declared length (always a compile-time
                              INT_LITERAL, ast_wrap_array's own `ival`),
                              N here can be any runtime expression, so
                              it's held as a full expression node rather
                              than an int. `new T[N]` and `new T[N](args)`
                              aren't distinguished from each other either
                              (constructor arguments alongside an array
                              size aren't accepted by this grammar at
                              all -- lower.c/codegen.c's array-`new`
                              support is allocation-only regardless; see
                              docs/DESIGN_NOTES.md). */
    AST_DELETE,           /* a=expr being deleted, ival=1 if this was
                              `delete[]` rather than plain `delete` (0
                              otherwise, via ast_new's calloc -- no
                              alternative ever needs to set this
                              explicitly). Both currently lower to the
                              same shape of call regardless of this flag
                              -- see lower.c's own doc comment on
                              new_delete_rewrite_expr's AST_DELETE case
                              for why array delete doesn't yet do
                              anything array-specific (no per-element
                              destructor invocation exists for either
                              new[] or delete[] yet). */
    AST_POINTER_TYPE,     /* a=pointee type -- represents "T *" */
    AST_REFERENCE_TYPE,   /* a=referent type -- represents "T &" */
    AST_ARRAY_TYPE,       /* a=element type, ival=length -- represents "T[N]"
                              on the C++ input side, in either accepted
                              declarator form (see parser.y's var_decl);
                              always emitted as Vircon32's own required
                              "ElementType [N]" form on output regardless
                              of which input form was used -- codegen.c's
                              print_type is where that happens */
    AST_INIT_LIST,        /* list=initializer values, e.g. the "{1, 2, 3}"
                              in "int arr[3] = {1, 2, 3};" -- ONLY ever
                              appears as a var_decl's own `a` (initializer),
                              and only for an array-typed one; this project
                              has no aggregate/struct initializer syntax of
                              its own, so this is array-specific, not a
                              general "braced initializer" concept */
    AST_CAST              /* type=target type, a=expr being cast -- an
                              explicit "(Type)expr". Never produced by the
                              parser (this project's grammar has no C-
                              style cast-expression syntax) -- introduced
                              by TWO lowering phases, both for the same
                              underlying reason: lower.c's finalize_call,
                              to make a receiver ("this") argument's
                              pointer type match whatever the callee
                              actually declares it as; and lower.c's
                              insert_pointer_cast_stmt (a later round),
                              to make a VarDecl's own pointer-typed
                              initializer match its declared type when
                              the two differ (e.g. `Shape *s = new
                              Square(4);`) -- confirmed directly that
                              Vircon32 rejects that implicit conversion
                              outright ("types are not compatible"),
                              unlike real C++. Both rely on the same
                              underlying guarantee: this project's
                              single-inheritance struct layout ensures a
                              derived class's fields are a valid prefix
                              of its base's, so the conversion is
                              genuinely SAFE either way -- C's type
                              system (and evidently Vircon32's own,
                              even more strictly than standard C) just
                              has no way to know that on its own, so an
                              explicit cast has to say so. */
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
     *   - AST_CALL -> CallResolution* (see sema.h), only if resolved
     * ast_new() zero-initializes this via calloc, so it's safely NULL
     * on every node until something sets it.
     */
    void *sema_info;

    /*
     * A SEPARATE opaque annotation slot, owned by lower.c rather than
     * sema.c -- kept distinct from sema_info specifically so lowering's
     * own output doesn't collide with (or have to overwrite) semantic
     * analysis's results, which lowering itself still needs to READ
     * while producing its own output (e.g. lower.c reads a class's
     * ClassLayout via sema_info while computing that same class's
     * StructLayout, stored here). Currently used by lower.c:
     *   - AST_CLASS_DECL -> StructLayout* (see lower.h)
     * Also zero-initialized (NULL) by ast_new().
     */
    void *lower_info;
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

/* Wraps `inner` as an array of `length` elements -- represents "T[N]"
 * regardless of which of the two accepted C++-side declarator forms
 * produced it (parser.y's var_decl has both); the AST itself carries no
 * memory of which spelling the source used. */
AstNode *ast_wrap_array(AstNode *inner, int length, int line);

void ast_dump(const AstNode *node, int indent);

#endif /* AST_H */
