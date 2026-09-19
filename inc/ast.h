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
                              members. ival=1 if declared with the
                              `struct` keyword, 0 for `class` -- the ONLY
                              difference between the two real C++ actually
                              has (default member access before any
                              explicit public:/private:/protected: label:
                              public for struct, private for class;
                              compute_layout() in sema.c reads this flag
                              for exactly that). Every other piece of this
                              project's own class machinery (vtables,
                              constructors, inheritance, access control)
                              applies identically to both -- a `struct`
                              with no methods at all already produces a
                              plain C struct in the generated output,
                              with no vtable/constructor overhead added,
                              since that machinery was always conditional
                              on the class actually having virtual
                              methods/constructors to begin with, not on
                              which keyword declared it. */
    AST_ACCESS_SPEC,      /* access=the new default access for what follows */
    AST_FRIEND_CLASS,     /* str1=friended class's name, as written --
                              `friend class X;` inside a class body. A
                              member-list entry, same as AST_ACCESS_SPEC
                              just above, but unlike it carries no access
                              level of its own and doesn't change
                              current_access for what follows -- friend
                              declarations are access-level-independent in
                              real C++ (writing one under `private:` vs
                              `public:` makes no difference at all), so
                              compute_layout() (sema.c) special-cases this
                              kind before it ever reaches the ordinary
                              "does this change current_access" branch.
                              Resolved to an actual AST_CLASS_DECL (via
                              find_class) and appended to the enclosing
                              class's own ClassLayout.friend_classes --
                              see that field's own doc comment (sema.h)
                              for exactly what granting friendship means
                              and its deliberate scope limits (not
                              transitive, not inherited). */
    AST_FRIEND_FUNC_DECL,  /* str1=name, type=return type, list=params --
                              `friend ReturnType f(params);` inside a
                              class body. Shares func_header's own AST
                              shape exactly (same fields an ordinary
                              AST_FUNC_DECL/AST_FUNC_DEF would have) --
                              deliberately a DIFFERENT AST kind rather
                              than an AST_FUNC_DECL with a "friend" flag
                              bit, so every existing switch in this
                              project that walks a class's own MEMBERS
                              (compute_layout, layout->methods,
                              this-injection, mangling, vtable-building,
                              ...) simply never sees it at all without
                              needing a new "is this a friend, skip it"
                              check bolted onto each one -- a friend
                              function is emphatically NOT a member: it
                              gets no `this` parameter, no mangled
                              ClassName__ prefix, no vtable slot, nothing.
                              Registered instead as an ordinary global
                              free-function declaration/candidate (the
                              exact same registry an out-of-class
                              `ReturnType f(params);` prototype already
                              populates) and, separately, by NAME, in the
                              enclosing class's own ClassLayout.
                              friend_function_names for access-checking
                              purposes -- see that field's own doc
                              comment (sema.h) for the name-only-matching
                              scope limit this implies. */
    AST_VAR_DECL,         /* str1=name, type=declared type, a=initializer or NULL.
                              access: meaningful only once sema.c has run and
                              this is a class member -- see compute_layout()
                              in sema.c, which stamps each member's access
                              level here as it walks the class body tracking
                              AST_ACCESS_SPEC markers (defaulting to
                              ACC_PRIVATE per `class`'s C++ default when no
                              marker precedes it). Meaningless/unset on a
                              free (non-member) variable declaration. */
    AST_VAR_DECL_GROUP,   /* list=two or more AST_VAR_DECL entries, sharing
                              one base type -- the real C/C++ "multiple
                              declarators in one statement" idiom
                              (`int a, b, c;`, `int a, *b, c = 5;`).
                              PURELY A PARSER-INTERNAL CARRIER: var_decl's
                              own plain-declarator production (parser.y)
                              is the only thing that ever builds one, and
                              every one of its own callers that folds a
                              var_decl into a surrounding list (top_decl_
                              list, a block's own statement list, member_
                              list, union_member_list) uses ast_list_
                              append_flatten (ast.c) instead of a plain
                              ast_list_append specifically so this node
                              expands back into its own several entries
                              right there, in the SAME grammar action that
                              built it -- no node of this kind ever
                              survives into the tree sema.c/lower.c/
                              codegen.c actually see, so none of those
                              files need a case for it at all. Deliberately
                              narrower than real C++'s own full declarator
                              grammar: only a PLAIN (pointer_opt-wrapped or
                              bare) declarator can appear after the first
                              comma -- an array or function-pointer
                              declarator mixed into a multi-declarator
                              statement (`int a, arr[8];`, `int a, (*fp)
                              (int);`) is a real, stated scope boundary,
                              not supported, matching this project's own
                              "narrower, deliberate boundary" pattern used
                              throughout (see var_decl's own grammar
                              comment in parser.y for the full reasoning). */
    AST_TYPEDEF_DECL,     /* str1=new name, type=underlying type */
    AST_ENUM_DECL,        /* str1=name, list=AST_ENUM_VALUE entries.
                              Top-level/namespace-level only -- NOT
                              supported as a class member (a nested
                              enum), a deliberate scope boundary, not an
                              oversight; see enum_decl's own comment in
                              parser.y. codegen.c emits this as literal,
                              unmodified C enum syntax -- no lowering
                              transformation at all, the same "Vircon32
                              C already has this natively" treatment
                              AST_SWITCH already gets (see its own doc
                              comment above). The enum's own NAME is
                              registered as a type (SYM_ENUM in
                              symtab.h, checked alongside SYM_CLASS/
                              SYM_TYPEDEF by the lexer's own TYPE_NAME
                              hack), so it can be used as an ordinary
                              variable/parameter type afterward, same as
                              a class or typedef name can be -- but
                              sema.c's own class-specific machinery
                              (ClassLayout, type_to_class, ...) has no
                              notion of it at all, since an enum isn't a
                              class; a variable of enum type is simply
                              never resolved to one, the same "unknown,
                              not an error" treatment any other
                              non-class type already gets. Each
                              enumerator NAME is never specially
                              resolved anywhere in this project's own
                              pipeline -- it flows through as an
                              ordinary bare AST_IDENT wherever it's
                              used, printed verbatim by codegen.c, and
                              made a legal, resolvable identifier
                              (equal to whatever integer value it
                              should have) purely by the fact that the
                              generated C's own enum declaration
                              defines it -- real C already does the
                              actual name-to-value resolution, so this
                              project doesn't have to. */
    AST_ENUM_VALUE,        /* str1=name, a=explicit value expr, or NULL
                              for "one more than the previous entry" (or
                              0, if this is the first) -- real C++'s own
                              auto-increment rule, needing no special
                              handling here since it's real C's own rule
                              too, applied automatically by whichever C
                              compiler processes the generated output;
                              this project never itself computes what an
                              omitted value resolves to. */
    AST_UNION_DECL,        /* str1=name, list=AST_VAR_DECL entries (the
                              union's own members -- reuses the exact
                              same node a struct/class field already
                              is, not a dedicated one, since a union
                              member is syntactically identical: a type
                              and a name). Top-level/namespace-level
                              only, same scope boundary as AST_ENUM_DECL
                              (no nested union-as-class-member support).
                              Deliberately NOT routed through class_decl
                              the way `struct` is -- real C++ itself
                              restricts what a union can contain (no
                              virtual functions, no base classes, no
                              vtable-requiring members at all), so
                              reusing class_decl's full machinery would
                              silently imply capabilities a union
                              doesn't actually have; this is its own,
                              narrower construct instead, sized to what
                              a union actually is. codegen.c emits this
                              as literal, unmodified C union syntax --
                              Vircon32 C already has this natively, the
                              same "pass it straight through" treatment
                              AST_SWITCH/AST_ENUM_DECL already get. */
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
                              chain the definition was written against.
                              str2=NULL normally; "const" (a literal,
                              non-NULL sentinel string, not meant to be
                              displayed) if a trailing `const` was written
                              after the parameter list (`int getValue()
                              const`) -- real C++'s own const-member-
                              function marker, meaning "this method
                              doesn't modify *this". Confirmed unused
                              elsewhere across this whole func_decl/
                              func_def/out_of_line_def family before
                              repurposing it for this, not assumed.
                              this_inject_method (lower.c) reads this to
                              decide whether the injected `this`
                              parameter's own type is `const ClassName *`
                              or plain `ClassName *` -- see its own
                              comment there. Like ival's virtual-ness,
                              this project accepts and threads the
                              keyword through correctly, but doesn't
                              ENFORCE what it promises: no error for a
                              const method actually modifying a field
                              through `this`. */
    AST_FUNC_DEF,         /* same as AST_FUNC_DECL but a=body (AST_BLOCK).
                              c=AST_MEMBER_INIT_LIST or NULL -- a constructor's
                              own member-initializer list (`: Base(args)`),
                              if one was written; NULL on every other kind
                              of AST_FUNC_DEF (ordinary method, destructor,
                              free function) and on a constructor that
                              didn't write one. See AST_MEMBER_INIT_LIST/
                              AST_MEMBER_INIT below for what it holds, and
                              sema.c's resolve_member_init_list for how each
                              entry gets validated and resolved. */
    AST_PARAM,            /* str1=name, type=param type */
    AST_MEMBER_INIT_LIST, /* list=AST_MEMBER_INIT entries, in the order
                              written -- only ever appears in an
                              AST_FUNC_DEF's own `c` slot (see there); never
                              constructed empty -- opt_member_init_list
                              (parser.y) produces NULL, not an empty list,
                              when no ": ..." was written at all, so `c`
                              being non-NULL always means at least one
                              entry. */
    AST_MEMBER_INIT,      /* str1=name -- either a base class's own name
                              (base-class-delegation: `: Base(args)`) or an
                              ordinary, primitive-typed member field's own
                              name (`: x(val)`) -- a class-typed member's
                              own name is also grammatically accepted here
                              (see below) but never acted on, since that's
                              real, separate complexity (invoking the
                              member's own constructor) this project
                              doesn't support anywhere yet.
                              Lexed as TYPE_NAME vs IDENTIFIER respectively
                              (same distinction this grammar already relies
                              on everywhere else), so the grammar itself
                              doesn't need to know which case it's parsing
                              -- that's sema.c's job.
                              list=constructor-call-style argument
                              expressions -- exactly one, for a resolved
                              member-field entry (real C++'s own
                              direct-initialization rule for a non-class
                              member); zero or more, for a resolved
                              base-class-delegation entry (whatever the
                              base's own matched constructor overload
                              takes).
                              sema_info=CallResolution* once
                              resolve_member_init_list has successfully
                              matched a base-class-delegation entry against
                              one of the base class's own constructor
                              overloads (see sema.h's CallResolution) --
                              NULL otherwise, including for a resolved
                              member-field entry (see ival below instead).
                              ival=1 once resolve_member_init_list has
                              successfully resolved a PRIMITIVE member-
                              field entry (name matches an actual data
                              member, that member's own type isn't a bare
                              class type, exactly one argument given) --
                              0 otherwise (a base-class-delegation entry,
                              which uses sema_info instead; a class-typed
                              member-field entry, not yet supported; or an
                              entry sema.c couldn't resolve at all).
                              lower.c's phase 8a reads ival==1 entries;
                              phase 8b reads sema_info != NULL entries --
                              the two are mutually exclusive by
                              construction, never both set on the same
                              entry. */
    AST_BLOCK,            /* list=statements */
    AST_IF,               /* a=cond, b=then-stmt, c=else-stmt or NULL */
    AST_WHILE,            /* a=cond, b=body. ival=1 for a do-while
                              (`do body while (cond);` -- test AFTER the
                              body runs once unconditionally, not
                              before), 0 for an ordinary `while` (test
                              before, per usual). Reuses this same node
                              kind rather than a separate AST_DO_WHILE
                              one, since a/b mean exactly the same thing
                              either way and every OTHER pass that walks
                              an AST_WHILE (sema.c's loop-depth tracking
                              for break/continue, lower.c's destructor-
                              boundary tracking) treats the two
                              identically -- a loop body is a loop body
                              regardless of when its condition is
                              tested; only codegen.c's own printing
                              needs to know the difference, checked
                              there via this same flag. */
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
    AST_GOTO,             /* str1=label name -- `goto label;`. codegen.c
                              emits this as the literal C keyword,
                              Vircon32 C already has this natively.
                              SCOPE: this project makes NO attempt to
                              validate that the named label actually
                              exists anywhere in the enclosing function,
                              nor does it track any of real C++'s own
                              restrictions on what a goto may jump
                              INTO or past (e.g. jumping into a block
                              past a variable's own initialization) --
                              left entirely to the downstream C
                              compiler to catch, the same best-effort
                              philosophy already applied elsewhere (an
                              unresolved global reference, an invalid
                              octal digit). Also, deliberately, no
                              destructor-invocation handling at a goto
                              the way break/continue/return already
                              get (see lower.c's own destruct_scope
                              phase) -- jumping out of a scope with a
                              live destructible local via goto is a
                              real, known gap, not silently assumed
                              safe; see this project's own README/
                              DESIGN_NOTES.md for the explicit call-out. */
    AST_LABEL,            /* str1=label name, a=the labeled statement --
                              `label: stmt`. Matches real C++'s own
                              grammar exactly: a label attaches to the
                              statement that follows it, it is not a
                              standalone thing or a container of its
                              own -- the same shape AST_SWITCH's own
                              case/default labels already have,
                              structurally, though those are handled by
                              a different pair of node kinds (AST_CASE/
                              AST_DEFAULT) since a switch's own body is
                              a flat list a label is injected into,
                              while an ordinary label instead wraps the
                              one statement it precedes. codegen.c
                              emits this as literal `label:` followed
                              by the statement, Vircon32 C already
                              having this natively too. */
    AST_SWITCH,           /* a=discriminant expr, list=body statements --
                              a FLAT list, matching real C's own switch-
                              body structure exactly: AST_CASE/AST_DEFAULT
                              are LABELS interleaved directly in this same
                              list, not separate containers holding their
                              own statements, so real C's fall-through
                              behavior (control continues into the NEXT
                              label's own statements unless something
                              stops it) falls out naturally from just
                              walking the list in order -- nothing this
                              project has to implement specially. `break`
                              inside the body exits the switch (see
                              AST_BREAK above); `continue` passes straight
                              through a switch to whatever loop (if any)
                              actually encloses it, matching real C's own
                              rule that continue never targets a switch.
                              codegen.c emits this as literal C
                              switch/case/default -- no lowering
                              transformation at all, since Vircon32 C
                              already has real, native switch/case. */
    AST_CASE,             /* a=case value (a constant expression) -- a
                              LABEL, not a container; see AST_SWITCH
                              above for why its own "body" is simply
                              whatever follows it in the enclosing
                              AST_SWITCH's own list. */
    AST_DEFAULT,          /* no fields -- a LABEL, same shape/rules as
                              AST_CASE. */
    AST_EXPR_STMT,        /* a=expr */
    AST_BINOP,            /* str1=operator text (including the bitwise
                              operators -- "&","|","^","<<",">>" -- which
                              flow through this same generic node exactly
                              like "+"/"-"/etc already do; codegen.c
                              prints str1 directly with no operator-
                              specific case needed, and sema.c's own
                              operator-overload resolution simply never
                              matches them, the same "always a plain
                              built-in op on primitives" treatment
                              "&&"/"||" already get -- see
                              binop_operator_name's own doc comment in
                              sema.c), a=lhs, b=rhs */
    AST_UNOP,             /* str1=operator text, a=operand */
    AST_ASSIGN,           /* str1=operator text ("=","+=",...,"&=","|=",
                              "^=","<<=",">>=" -- the bitwise compound-
                              assignment forms flow through this same
                              generic node too, same reasoning as
                              AST_BINOP above), a=lhs, b=rhs */
    AST_TERNARY,          /* a=condition, b=true-branch, c=false-branch --
                              `cond ? true_branch : false_branch`.
                              Grammar-level precedence sits between
                              assignment (loosest) and `||` (see the new
                              '?' precedence declaration in parser.y),
                              matching real C++'s own conditional-
                              expression placement; right-associative, so
                              a chained `a ? b : c ? d : e` parses as
                              `a ? b : (c ? d : e)`, same as real C++.
                              codegen.c prints this as a literal C
                              ternary -- Vircon32 C already has this
                              natively, so no lowering transformation
                              happens here at all, the same "pass it
                              through" treatment AST_SWITCH already
                              gets. */
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
    AST_NULL_LIT,         /* `nullptr` -- no payload fields at all, same as
                              AST_THIS just below. A distinct kind from
                              AST_INT_LIT (rather than reusing it with
                              ival=0) for the same reason AST_BOOL_LIT is
                              its own kind and not just an AST_INT_LIT
                              with ival 0/1: codegen needs to print this
                              one specific value differently from an
                              ordinary integer zero -- Vircon32's own C
                              compiler requires the literal word `NULL`
                              for a null pointer constant and rejects a
                              bare `0` in pointer context outright (see
                              docs/VIRCON32_QUIRKS.md), so `nullptr`
                              always prints as `NULL` (misc.h's own
                              macro, already `#include`d unconditionally
                              for every Vircon32-mode program) regardless
                              of what pointer type it's initializing --
                              exactly mirroring real C++'s own nullptr,
                              which likewise carries no type of its own
                              until context supplies one. */
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
    AST_DIRECT_INIT,      /* Marker node ONLY ever found sitting in an
                              AST_VAR_DECL's own `a` slot (never anywhere
                              an ordinary expression is expected) --
                              `Shape shape(7);`'s constructor-argument
                              list, `list=(7)`, type=NULL until sema.c's
                              overload resolution fills it in (see below).
                              Distinct from AST_NEW specifically because
                              this is a STACK-allocated local's direct-
                              initialization, not a heap allocation --
                              new_delete_rewrite_expr's whole allocator-
                              naming machinery doesn't apply here at all,
                              there's no pointer being returned, so this
                              needed its own node kind rather than reusing
                              AST_NEW's (which lower.c's phase 6
                              unconditionally rewrites into a
                              "v32_new_ClassName" heap-allocator call --
                              exactly the wrong shape for a value that
                              already has its own storage on the stack).
                              sema.c's check_node fills in `type` with the
                              VAR_DECL's own declared class type (needed
                              because resolve_new_expr/resolve_overload_
                              generic, reused unchanged for this node kind
                              too, reads the class to resolve constructor
                              overloads against from `node->type`, exactly
                              like it does for an AST_NEW) and attaches a
                              CallResolution* to `sema_info` exactly like
                              AST_NEW does, resolving which constructor
                              overload these arguments match -- same
                              diagnostics, same "no matching overload"/
                              "ambiguous" errors. lower.c's phase 7
                              (inject_ctor_calls) reads this resolution
                              and emits a direct call to that constructor's
                              own mangled name with `&shape` as the
                              receiver, then clears the VAR_DECL's `a`
                              slot back to NULL (this node never survives
                              to codegen -- there's nothing for codegen to
                              print here, the initializer becomes a
                              separate statement immediately after the
                              declaration, the same shape phase 7's
                              existing zero-argument constructor injection
                              already produces). */
    AST_POINTER_TYPE,     /* a=pointee type -- represents "T *" */
    AST_REFERENCE_TYPE,   /* a=referent type -- represents "T &" */
    AST_CONST_TYPE,       /* a=underlying type -- represents "const T"
                              (pointee-const / variable-const, e.g.
                              `const int x`, `const int *p` -- a
                              pointer to const int). Wraps its own
                              inner type the same way AST_POINTER_TYPE/
                              AST_REFERENCE_TYPE/AST_ARRAY_TYPE already
                              do, but PREFIXES rather than suffixes when
                              printed (`const int`, not `int const`) --
                              codegen.c's print_type has its own case
                              for this reason, it isn't just another
                              "recurse then append" wrap. SCOPE: only
                              ever wraps the type itself (`const int`,
                              `const int *` -- pointer TO const, the
                              pointee can't change); a const POINTER
                              itself (`int * const p` -- the pointer
                              can't be reassigned, its pointee can) is
                              NOT supported -- `const` is only accepted
                              as a PREFIX before a type_spec, never
                              after a `pointer_opt`'s own `*`. This
                              project makes no attempt to actually
                              ENFORCE const-correctness anywhere either
                              (no error for reassigning a const
                              variable, no error for calling a non-
                              const method through a const reference) --
                              accepted and emitted correctly in the
                              generated C so it doesn't block valid
                              code from transpiling at all, with real
                              const-correctness violations left for the
                              downstream C/C++ compiler to catch, the
                              same best-effort philosophy this project
                              already applies elsewhere. */
    AST_ARRAY_TYPE,       /* a=element type, ival=length -- represents "T[N]"
                              on the C++ input side, in either accepted
                              declarator form (see parser.y's var_decl);
                              always emitted as Vircon32's own required
                              "ElementType [N]" form on output regardless
                              of which input form was used -- codegen.c's
                              print_type is where that happens.
                              Multi-dimensional arrays are simply NESTED
                              AST_ARRAY_TYPE nodes -- `int grid[8][4]`'s
                              own type is an AST_ARRAY_TYPE (ival=8)
                              whose own element type (a) is ANOTHER
                              AST_ARRAY_TYPE (ival=4) wrapping plain
                              `int` -- the outermost node carries the
                              FIRST bracket's length, matching real C's
                              own "array of arrays" semantics exactly
                              (`grid` is an array of 8 elements, each of
                              which is an array of 4 ints, not the
                              reverse). No new node kind needed for
                              this -- see ast_wrap_array_dims (ast.c)
                              for how parser.y builds the correct
                              nesting from a source-order list of
                              bracket lengths, and print_type's own
                              existing recursion (already needed for
                              "array of function pointers") handles
                              printing it correctly with no changes at
                              all once it existed for that case. */
    AST_FUNC_PTR_TYPE,    /* type=return type, list=param TYPES (bare
                              types only -- e.g. a list of AST_IDENT/
                              AST_POINTER_TYPE/... nodes built from
                              type_spec+pointer_opt, never full "param"
                              nodes with names, since a function-pointer
                              TYPE carries no parameter names at all,
                              matching real C++ exactly) -- represents
                              the TYPE of a function pointer, e.g.
                              "int (*)(int, int)" on the standard-C
                              side. Accepted on the C++ input side in
                              BOTH standard-C declarator form
                              ("ReturnType (*name)(ParamTypes);") and
                              Vircon32-native form
                              ("ReturnType(ParamTypes)* name;" -- see
                              docs/VIRCON32_QUIRKS.md's own "Function-
                              pointer declarator syntax reversed" entry,
                              itself confirmed against the real
                              compiler via vtable-slot emission, long
                              before this node existed to represent a
                              SOURCE-level declaration of the same
                              shape) -- same "two accepted spellings,
                              one AST shape, one always-Vircon32-style
                              output" treatment AST_ARRAY_TYPE already
                              established, extended here deliberately
                              rather than decided fresh (see
                              VIRCON32_QUIRKS.md's own standing
                              principle on this). Composes with
                              AST_ARRAY_TYPE for free, no special
                              casing needed anywhere: an "array of
                              function pointers" is simply an
                              AST_ARRAY_TYPE whose own element type (a)
                              is an AST_FUNC_PTR_TYPE, exactly the same
                              structural relationship an ordinary array
                              of ints already has -- ast_wrap_array
                              doesn't care what it's wrapping, and
                              print_type's own recursion handles it
                              automatically once AST_FUNC_PTR_TYPE has
                              its own case. codegen.c's print_type
                              emits the return type, then "(ParamType,
                              ParamType, ...)", then "*" -- with NO
                              name embedded inside the parens at all
                              (unlike the standard-C form's own
                              "(*name)"), matching the confirmed
                              vtable-slot precedent exactly
                              (`int(Shape *)* Shape__area__void;`) --
                              the caller then appends " name" after
                              print_type returns, the same pattern
                              every other type already follows. */
    AST_INIT_LIST,        /* list=initializer values, e.g. the "{1, 2, 3}"
                              in "int arr[3] = {1, 2, 3};" -- ONLY ever
                              appears as a var_decl's own `a` (initializer),
                              and only for an array-typed one; this project
                              has no aggregate/struct initializer syntax of
                              its own, so this is array-specific, not a
                              general "braced initializer" concept */
    AST_CAST,             /* type=target type, a=expr being cast.
                              ival=1 if this was specifically written as
                              `dynamic_cast<T>(...)` (see cpp_cast_kw in
                              parser.y) -- 0 for every other spelling
                              (C-style `(Type)expr`, `static_cast`,
                              `const_cast`, `reinterpret_cast`), all of
                              which this project treats identically:
                              real C++'s own distinctions between them
                              (static_cast/const_cast/reinterpret_cast
                              are all compile-time-only, no runtime
                              check for any of them) collapse to nothing
                              once the target is C, which has no notion
                              of any of these cast KINDS at all, only a
                              single, generic cast syntax -- Vircon32 C
                              included. dynamic_cast is different: real
                              C++ gives it an actual runtime type check
                              (returning NULL on a failed pointer cast),
                              which requires RTTI -- this project has
                              never supported RTTI, by design, so a
                              user-written dynamic_cast transpiles as a
                              bare, ordinary cast too, NOT a real,
                              safety-checked one; ival=1 exists so
                              sema.c's own check_node can warn about
                              that gap specifically (see its AST_CAST
                              case) rather than silently accepting code
                              that looks safety-checked but isn't.

                              Reachable from user-written source (see
                              unary_expr's own cast alternatives in
                              parser.y) as well as being synthesized
                              internally by TWO lowering phases, both
                              for the same underlying reason: lower.c's
                              finalize_call,
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
    AST_SIZEOF             /* Exactly one of type/a is set, never both --
                              real C++'s own dual grammar for sizeof:
                              type=target type for `sizeof(Type)`
                              (parens required for this form in real
                              C++ too), a=target expr for `sizeof expr`
                              or `sizeof(expr)` (parens optional --
                              `sizeof(expr)` reaches this same a-set
                              form via unary_expr's own reduction
                              through primary_expr's `'(' expr ')'`,
                              not through the type-taking alternative,
                              since parser.y's own two productions are
                              disambiguated the identical way the
                              C-style cast's two possible readings
                              already are: type_spec's own first-set
                              -- TYPE_NAME, INT_KW, FLOAT_KW, ... --
                              never overlaps with expr's). codegen.c
                              prints this as literal `sizeof(...)` --
                              Vircon32 C already has this natively, the
                              same "pass it straight through" treatment
                              AST_SWITCH/AST_ENUM_DECL/AST_UNION_DECL
                              already get; this project never itself
                              computes a size, real C's own compiler
                              does, downstream. */
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
void ast_list_append_flatten(AstList *list, AstNode *node); /* see its own
    doc comment in ast.c -- expands an AST_VAR_DECL_GROUP into its own
    entries instead of appending it as one */

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
AstNode *ast_wrap_const(AstNode *inner, int line);

/* Wraps `inner` as an array of `length` elements -- represents "T[N]"
 * regardless of which of the two accepted C++-side declarator forms
 * produced it (parser.y's var_decl has both); the AST itself carries no
 * memory of which spelling the source used. */
AstNode *ast_wrap_array(AstNode *inner, int length, int line);
AstNode *ast_wrap_array_dims(AstNode *inner, AstList dims, int line);
AstNode *ast_wrap_func_ptr(AstNode *return_type, AstList param_types, int line);

void ast_dump(const AstNode *node, int indent);

#endif /* AST_H */
