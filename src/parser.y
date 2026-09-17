/*
 * parser.y -- GLR grammar for the transpiler's C++ subset.
 *
 * Scope of this skeleton: namespaces, classes with single inheritance and
 * public/private/protected sections, constructors/destructors, virtual
 * functions (parsed but not yet lowered to vtables -- that's a later pass),
 * function/operator overloading (parsed; overload *resolution* is a later
 * semantic pass), references and pointers as declarator modifiers, new/
 * delete, and a conventional C-like statement/expression language.
 *
 * Explicitly NOT handled here: templates, exceptions, RTTI, multiple
 * inheritance, the preprocessor (run your input through `cpp` or an
 * equivalent first -- see the Makefile), operator-overload *declarator*
 * syntax (`operator+`, etc. -- straightforward to add, just not in yet).
 *
 * %glr-parser is declared so the grammar can grow into genuinely ambiguous
 * C++ declarator territory (function-pointer types, the "most vexing
 * parse") later without needing a parser-generator switch. As it happens,
 * GLR isn't even doing real forking work for anything in this grammar
 * today -- the remaining shift/reduce conflicts (see the comment where
 * %expect used to be pinned, a few lines down) are all resolved by
 * bison's default shift preference, not by parser forking. The typedef/
 * class-name-vs-value disambiguation that *does* need active resolution is
 * handled entirely by the lexer (see lexer.l and symtab.h), which is what
 * keeps `Foo * x;` unambiguous without the parser needing to do anything
 * special with it.
 */

%{
/*
 * Classic yacc-style prologue instead of %code requires/%code top: some
 * systems (notably macOS, where /usr/bin/bison is frozen at GNU bison 2.3
 * for licensing reasons) predate %code, which bison only added in 2.4.
 * This form works on every bison version, old or new.
 *
 * NOTE: unlike %code requires, this block is emitted into parser.tab.c
 * but is NOT copied into the generated parser.tab.h. That's fine here:
 * the union below only needs AstNode/AstList/Symbol/etc. to be *visible*,
 * and every other file that includes parser.tab.h (lexer.l, main.c)
 * already includes driver.h -- which pulls in ast.h and symtab.h -- first.
 * If you add a new .c file that includes parser.tab.h directly, make sure
 * it includes driver.h (or ast.h+symtab.h) immediately before it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symtab.h"
#include "driver.h"
%}

%glr-parser
%locations
%define parse.error verbose

/*
 * 22 shift/reduce conflicts -- re-verified after fixing a real bug in
 * out_of_line_def's constructor rule (see the long comment on that
 * alternative below, and docs/DESIGN_NOTES.md, for the postmortem:
 * `Counter::Counter(...)` used to fail to parse entirely, which is a
 * different and worse thing than a benign shift/reduce conflict).
 *
 * The count landing back at 22 is not assumed to be a coincidence carried
 * over from before the fix -- it was re-checked against fresh
 * `bison -Wcounterexamples` output for this exact grammar. Specifically:
 * every one of the 22 derivation trees routes through either the
 * regular out-of-line method alternative (`type_spec qname_prefix
 * IDENTIFIER '(' ...`) or the destructor alternative (`qname_prefix '~'
 * TYPE_NAME '(' ')'`), both already part of the documented family below
 * (opt_virtual's nullable prefix vs. type_spec/qname_prefix-starting
 * alternatives, across the same three contexts: global scope, a
 * namespace body, a class body). The FIXED constructor alternative
 * (`qualified_type '(' ...`) does not appear in any of the 22 -- meaning
 * that specific fork is now fully and unambiguously resolved, not merely
 * hidden behind a passing conflict count.
 *
 * This is safe in every instance: bison's default is to prefer shift,
 * i.e. defer the decision rather than commit early, and the real fork
 * only needs one more token of lookahead anyway -- `(` right after
 * IDENTIFIER/TYPE_NAME means "this was a function/constructor/destructor
 * name", anything else means "this was a return type, keep going." Plain
 * 1-token lookahead, so no input can actually be misparsed; the conflict
 * is purely an artifact of *when* the tables notice the ambiguity, not a
 * parsing hazard. See the comment on opt_virtual/func_header, and the one
 * on out_of_line_def, for the grammar-level version of this explanation.
 *
 * If a future grammar edit changes this count, bison will error (rather
 * than warn) until %expect is updated -- treat that as a prompt to
 * re-run `bison -Wcounterexamples`, read every new counterexample (not
 * just the total, and not just assuming it matches a known-benign shape
 * -- see the postmortem for why that assumption failed once already),
 * and confirm the actual concrete input you care about still parses
 * correctly before trusting the new number.
 */
%expect 22

%union {
    AstNode *node;
    AstList list;
    char *str;
    int ival;
    double fval;
    AccessSpec access;
}

%token <str> IDENTIFIER TYPE_NAME STRING_LITERAL
%token <ival> INT_LITERAL CHAR_LITERAL
%token <fval> FLOAT_LITERAL

%token CLASS STRUCT PUBLIC PRIVATE PROTECTED NAMESPACE TYPEDEF
%token RETURN IF ELSE WHILE FOR BREAK CONTINUE
%token SWITCH CASE DEFAULT
%token INT_KW FLOAT_KW VOID_KW BOOL_KW CHAR_KW
%token NEW DELETE THIS VIRTUAL TRUE_KW FALSE_KW OPERATOR
%token STATIC_CAST DYNAMIC_CAST CONST_CAST REINTERPRET_CAST
%token COLONCOLON ARROW EQ NE LE GE ANDAND OROR
%token PLUSEQ MINUSEQ STAREQ SLASHEQ INC DEC
%token SHL SHR ANDEQ OREQ XOREQ SHLEQ SHREQ

%type <node> program top_decl namespace_decl class_decl member
%type <node> func_decl func_def func_header var_decl typedef_decl out_of_line_def
%type <node> opt_member_init_list member_init
%type <node> block stmt for_init opt_initializer opt_array_initializer
%type <node> expr expr_opt unary_expr postfix_expr primary_expr
%type <node> qualified_id_expr qualified_type type_spec param opt_base

%type <list> top_decl_list member_list stmt_list
%type <list> param_list opt_param_list arg_list opt_arg_list qname_prefix
%type <list> member_init_list
%type <list> switch_body

%type <str> name_tok func_name operator_symbol
%type <access> access_spec
%type <ival> pointer_opt opt_virtual class_or_struct_kw cpp_cast_kw

%right '=' PLUSEQ MINUSEQ STAREQ SLASHEQ ANDEQ OREQ XOREQ SHLEQ SHREQ
%left OROR
%left ANDAND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left '<' '>' LE GE
%left SHL SHR
%left '+' '-'
%left '*' '/' '%'

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%start program

%%

program:
    top_decl_list
        {
            g_program = ast_new(AST_PROGRAM, 1);
            g_program->list = $1;
            $$ = g_program;
        }
    ;

top_decl_list:
      /* empty */               { $$ = ast_list_new(); }
    | top_decl_list top_decl    { $$ = $1; ast_list_append(&$$, $2); }
    ;

top_decl:
      namespace_decl    { $$ = $1; }
    | class_decl ';'    { $$ = $1; }
    | func_def          { $$ = $1; }
    | func_decl ';'     { $$ = $1; }
    | var_decl ';'       { $$ = $1; }
    | typedef_decl ';'   { $$ = $1; }
    | out_of_line_def    { $$ = $1; }
    ;

/* ---- namespaces --------------------------------------------------- */

namespace_decl:
    NAMESPACE IDENTIFIER
        {
            /* Namespaces are reopenable: `namespace v32 { ... }` appearing
             * twice in the same TU should extend the same member set, not
             * create two disjoint ones (this matters for the hardware
             * namespaces -- v32::, ioports:: -- which real programs will
             * reopen across multiple headers).
             *
             * SIMPLIFICATION: this assumes a namespace is always reopened
             * at the same lexical nesting depth as its first opening (true
             * for flat top-level hardware namespaces). A production
             * version would separate "lexical parent scope" from
             * "namespace member set" the way real compilers do, so a
             * namespace could be legally reopened at different points
             * without corrupting the scope-pop parent chain. */
            Symbol *nsym = symtab_lookup_in(g_symtab->current, $2);
            if (nsym == NULL) {
                nsym = symtab_insert(g_symtab, g_symtab->current, $2, SYM_NAMESPACE);
            }
            if (nsym->inner_scope == NULL) {
                symtab_push_scope(g_symtab, $2, 0);
                nsym->inner_scope = g_symtab->current;
            } else {
                g_symtab->current = nsym->inner_scope;
            }
        }
    '{' top_decl_list '}'
        {
            symtab_pop_scope(g_symtab);
            $$ = ast_new(AST_NAMESPACE_DECL, @1.first_line);
            $$->str1 = strdup($2);
            $$->list = $5;
        }
    ;

/* ---- classes -------------------------------------------------------- */

class_decl:
    class_or_struct_kw IDENTIFIER opt_base
        {
            /* Register the class *before* the body is scanned, so that
             * self-referential members (`Node *next;`) and constructor/
             * destructor declarations -- which re-mention the class's own
             * name, now classified as TYPE_NAME by the lexer -- resolve
             * correctly. See driver.h for the single-class-at-a-time
             * caveat (no nested classes yet). */
            g_current_class_sym = symtab_insert(g_symtab, g_symtab->current, $2, SYM_CLASS);
            symtab_push_scope(g_symtab, $2, 1);
            g_current_class_sym->inner_scope = g_symtab->current;

            /* Injected class name (C++ [class.pre]): within its own body,
             * a class's name is implicitly visible as if it were a member
             * of itself. Without this, a self-qualified reference like
             * `Counter::Counter` breaks: the "::" arms pending_qualifier
             * to look *inside* Counter's own scope, but the constructor
             * rule (TYPE_NAME '(' ...) never inserts "Counter" as a
             * symbol under its own name -- it doesn't need to for the
             * in-class case, since it's just reusing the already-
             * registered class name. So the lookup for the second
             * "Counter" in an out-of-line `Counter::Counter(...)` or
             * `Counter::~Counter()` fails, falls back to plain
             * IDENTIFIER, and the parser -- correctly, given that input
             * -- rejects it expecting another "::". Confirmed against a
             * real failing build and a hand-traced parser.output before
             * landing on this as the actual root cause; see
             * docs/DESIGN_NOTES.md for the full story, including why an
             * earlier, different fix (in out_of_line_def's grammar) was
             * necessary but not sufficient on its own. */
            Symbol *injected = symtab_insert(g_symtab, g_symtab->current, $2, SYM_CLASS);
            injected->inner_scope = g_symtab->current;
        }
    '{' member_list '}'
        {
            symtab_pop_scope(g_symtab);
            $$ = ast_new(AST_CLASS_DECL, @1.first_line);
            $$->str1 = strdup($2);
            $$->str2 = $3 ? strdup($3->str1) : NULL;
            /* Inheritance access-specifier (public/private/protected),
             * carried on $3 (see opt_base) -- meaningless when $3/str2 is
             * NULL (no base at all), but ACC_PUBLIC is a harmless inert
             * default for that case rather than leaving it uninitialized. */
            $$->access = $3 ? $3->access : ACC_PUBLIC;
            $$->list = $6;
            $$->ival = $1; /* is_struct -- see class_or_struct_kw below and
                AST_CLASS_DECL's own doc comment in ast.h for what this
                controls (only the default member-access level; every
                other piece of this project's class machinery applies
                identically either way) */
            g_current_class_sym = NULL;
        }
    ;

/* Distinguishes `class` from `struct` at the very first token of
 * class_decl -- real C++'s only actual difference between the two
 * (default member access before any explicit public:/private:/
 * protected: label) lives entirely in this single ival value, read by
 * sema.c's compute_layout(); nothing else in this grammar, sema.c's
 * layout computation, lower.c, or codegen.c needs to know or care which
 * keyword originally declared a given class. */
class_or_struct_kw:
      CLASS   { $$ = 0; }
    | STRUCT  { $$ = 1; }
    ;

opt_base:
      /* empty */                { $$ = NULL; }
    | ':' PUBLIC TYPE_NAME
        {
            /* Represented as a plain AST_IDENT carrying the base name in
             * str1 and the inheritance access-specifier in ->access --
             * reusing ast_ident() rather than adding a new semantic-value
             * type just to pair a string with an enum. class_decl's
             * action below unpacks both fields. */
            $$ = ast_ident($3, @3.first_line);
            $$->access = ACC_PUBLIC;
        }
    | ':' PRIVATE TYPE_NAME
        {
            $$ = ast_ident($3, @3.first_line);
            $$->access = ACC_PRIVATE;
        }
    | ':' PROTECTED TYPE_NAME
        {
            $$ = ast_ident($3, @3.first_line);
            $$->access = ACC_PROTECTED;
        }
    ;

member_list:
      /* empty */          { $$ = ast_list_new(); }
    | member_list member   { $$ = $1; ast_list_append(&$$, $2); }
    ;

member:
      access_spec ':'
        {
            $$ = ast_new(AST_ACCESS_SPEC, @1.first_line);
            $$->access = $1;
        }
    | func_decl ';'   { $$ = $1; }
    | func_def        { $$ = $1; }
    | var_decl ';'     { $$ = $1; }
    ;

access_spec:
      PUBLIC     { $$ = ACC_PUBLIC; }
    | PRIVATE    { $$ = ACC_PRIVATE; }
    | PROTECTED  { $$ = ACC_PROTECTED; }
    ;

/* ---- functions -------------------------------------------------------
 *
 * func_header pushes a fresh "function scope" right after '(' so that
 * parameters are visible for lookup while the parameter list itself is
 * being parsed, and (for func_def) remain visible while the body's block
 * is parsed -- block's own scope nests *inside* this one, so params are
 * found via the normal enclosing-scope walk. func_decl pops this scope
 * immediately (no body will ever use it); func_def pops it after the body
 * has been fully parsed.
 */

opt_virtual:
      /* empty */  { $$ = 0; }
    | VIRTUAL      { $$ = 1; }
    ;
/* opt_virtual's ε alternative is the direct cause of 21 of this grammar's
 * shift/reduce conflicts (see the %expect comment at the top of the file
 * for the full explanation). In short: because opt_virtual can match
 * nothing, func_decl/func_def (= opt_virtual func_header) and var_decl
 * (which has no such prefix) both effectively "start" at type_spec from
 * the parser's point of view, and the tables can't tell which rule they're
 * in until they see whether '(' follows the first IDENTIFIER. Bison's
 * default shift resolves this correctly by deferring that decision rather
 * than committing early -- nothing to fix here, just something to
 * remember if this area of the grammar gets restructured later. */

/* ---- operator overloading: `operator+`, `operator==`, `operator[]`, etc.
 * are just another spelling of "the function's own name" wherever
 * func_header spells one out of IDENTIFIER -- func_name below is a drop-in
 * replacement for IDENTIFIER in exactly those spots (func_header's first
 * alternative here, and out_of_line_def's regular-method alternative),
 * producing an ordinary string like "operator+" that flows through
 * everything downstream (attach_out_of_line matching, vtable slot keys)
 * exactly like any other function name. The one place that DOES need to
 * know it's an operator is sema.c's mangle(), which maps it to a
 * C-identifier-safe fragment (op_add, op_eq, ...) the same way it already
 * maps a destructor's "~Foo" to "dtor" -- see mangle_operator_symbol()
 * there.
 *
 * Deliberately supported: binary +, -, *, /; comparisons ==, !=, <, >,
 * <=, >=; compound assignment +=, -=, *=, /=; plain assignment =; unary
 * !; subscript []; call (). Unary vs. binary +/- needs no special
 * grammar handling at all -- it falls out entirely from how many
 * parameters the surrounding param list happens to have, exactly the
 * same way it would for any other overloaded method name.
 *
 * Deliberately NOT supported yet:
 *   - `<<`/`>>` -- this project doesn't have shift-operator TOKENS at all
 *     yet (not even as ordinary bitwise operators in `expr`), so there's
 *     nothing for operator_symbol to reuse; would need its own lexer/expr
 *     work first, unrelated to operator overloading specifically.
 *   - `++`/`--` -- real C++ disambiguates prefix from postfix via a
 *     dummy, otherwise-meaningless `int` parameter on the postfix form
 *     (`T operator++(int)`), which is a genuine special case worth
 *     handling deliberately rather than folding in as an afterthought.
 *   - `operator ReturnType()` (user-defined conversion operators) --
 *     structurally different (no separate return-type token at all,
 *     which is what everything else here assumes exists).
 */

func_name:
      IDENTIFIER            { $$ = $1; }
    | OPERATOR operator_symbol   { $$ = $2; }
    ;

operator_symbol:
      '+'       { $$ = strdup("operator+"); }
    | '-'       { $$ = strdup("operator-"); }
    | '*'       { $$ = strdup("operator*"); }
    | '/'       { $$ = strdup("operator/"); }
    | '='       { $$ = strdup("operator="); }
    | '!'       { $$ = strdup("operator!"); }
    | EQ        { $$ = strdup("operator=="); }
    | NE        { $$ = strdup("operator!="); }
    | '<'       { $$ = strdup("operator<"); }
    | '>'       { $$ = strdup("operator>"); }
    | LE        { $$ = strdup("operator<="); }
    | GE        { $$ = strdup("operator>="); }
    | PLUSEQ    { $$ = strdup("operator+="); }
    | MINUSEQ   { $$ = strdup("operator-="); }
    | STAREQ    { $$ = strdup("operator*="); }
    | SLASHEQ   { $$ = strdup("operator/="); }
    | '[' ']'   { $$ = strdup("operator[]"); }
    | '(' ')'   { $$ = strdup("operator()"); }
    ;

func_header:
      type_spec func_name '(' { symtab_push_scope(g_symtab, NULL, 0); } opt_param_list ')'
        {
            /* Overload note: this inserts every overload of `name` into
             * the same bucket, later ones shadowing earlier ones for
             * lookup purposes. That's harmless for this skeleton, since
             * the symbol table is only consulted for type-vs-value
             * classification here, not signature matching -- but real
             * overload *resolution* (picking the right overload at a call
             * site) needs a proper per-scope overload set, which belongs
             * in the semantic-analysis pass over the AST, not in this
             * table. */
            symtab_insert(g_symtab, g_symtab->current->parent, $2, SYM_FUNC);
            $$ = ast_new(AST_FUNC_DECL, @2.first_line);
            $$->str1 = strdup($2);
            $$->type = $1;
            $$->list = $5;
        }
    | TYPE_NAME '(' { symtab_push_scope(g_symtab, NULL, 0); } opt_param_list ')'
        {
            /* Constructor: the name token is TYPE_NAME because it's the
             * enclosing class's own (already-registered) name -- see
             * class_decl above. No return type. */
            $$ = ast_new(AST_FUNC_DECL, @1.first_line);
            $$->str1 = strdup($1);
            $$->type = NULL;
            $$->list = $4;
        }
    | '~' TYPE_NAME '(' ')'
        {
            symtab_push_scope(g_symtab, NULL, 0); /* kept for symmetry with the pop in func_decl/func_def */
            $$ = ast_new(AST_FUNC_DECL, @1.first_line);
            size_t len = strlen($2) + 2;
            char *dtor_name = malloc(len);
            snprintf(dtor_name, len, "~%s", $2);
            $$->str1 = dtor_name;
            $$->type = NULL;
            $$->list = ast_list_new();
        }
    ;

func_decl:
    opt_virtual func_header
        {
            $$ = $2;
            $$->ival = $1; /* virtual-ness, per parsing -- see AST_FUNC_DECL
                             * in ast.h. Note this reflects only what was
                             * literally written here; sema.c's vtable
                             * builder may ALSO set this to 1 on a class
                             * whose method silently overrides an inherited
                             * virtual slot without repeating the keyword
                             * (matching real C++), so by the time sema has
                             * run, ival means "is this virtual", not just
                             * "was 'virtual' written on this exact line". */
            symtab_pop_scope(g_symtab); /* prototype only; no body needs the param scope */
        }
    ;

func_def:
    opt_virtual func_header opt_member_init_list block
        {
            $$ = $2;
            $$->kind = AST_FUNC_DEF;
            $$->ival = $1; /* see the comment in func_decl above */
            $$->c = $3;    /* member-initializer list, or NULL -- see
                               opt_member_init_list's own doc comment.
                               Grammatically reachable after ANY func_header
                               (ordinary method, destructor, constructor
                               alike), not just a constructor's -- sema.c's
                               resolve_member_init_list is what actually
                               rejects a non-constructor writing one, the
                               same "parser accepts the shape, sema.c
                               diagnoses the misuse" split this grammar
                               already relies on elsewhere (e.g. `break`
                               outside a loop is a parse-time non-issue,
                               caught by sema.c instead). */
            $$->a = $4;
            symtab_pop_scope(g_symtab);
        }
    ;

/* ---- out-of-line member definitions: ReturnType Class::method(...) {},
 * Class::Class(...) {} (constructor), Class::~Class() {} (destructor).
 *
 * SCOPE: single-level qualifiers only, matching the rest of this skeleton
 * (no nested classes). `qname_prefix` will happily parse a longer chain
 * like `v32::Timer::method`, but the semantic-analysis pass currently
 * resolves against a *flat* class registry keyed by the class's bare
 * name, using only the *last* component of the chain -- so a namespaced
 * class's out-of-line definitions aren't correctly scoped yet. See the
 * TODO in sema.c's attach_out_of_line() before relying on that case.
 *
 * These productions don't try to verify the qualifier actually names a
 * real, previously-declared class, or that a matching prototype exists in
 * it -- that's exactly the job handed to sema_run() in sema.c. The parser
 * just records the qualifier chain (as an AST_QUALIFIED_ID) on the node's
 * `b` slot -- see the AST_FUNC_DECL/AST_FUNC_DEF comments in ast.h -- and
 * gets out of the way.
 *
 * CONFLICT NOTE, CORRECTED: an earlier version of the constructor
 * alternative below duplicated qualified_type's exact right-hand side
 * (`qname_prefix TYPE_NAME`) as an inline sequence rather than reusing
 * qualified_type itself, and that turned out to be a genuine parser bug,
 * not a benign conflict -- confirmed by an actual failing build on
 * `Counter::Counter(...)`. See the postmortem comment on the constructor
 * alternative itself, and docs/DESIGN_NOTES.md, for the full story. The
 * fix routes through `qualified_type`, which reduces this to the same
 * already-proven-safe fork the rest of the file relies on. `%expect` is
 * temporarily removed a few lines up pending a fresh, verified conflict
 * count against this corrected grammar -- don't assume any particular
 * number until `bison -Wcounterexamples` has actually been run against
 * this version.
 */

out_of_line_def:
    type_spec qname_prefix func_name '(' { symtab_push_scope(g_symtab, NULL, 0); } opt_param_list ')' block
        {
            $$ = ast_new(AST_FUNC_DEF, @1.first_line);
            $$->str1 = strdup($3);
            $$->type = $1;
            $$->list = $6;
            $$->a = $8;
            $$->b = ast_new(AST_QUALIFIED_ID, @2.first_line);
            $$->b->list = $2;
            symtab_pop_scope(g_symtab);
        }
    | qualified_type '(' { symtab_push_scope(g_symtab, NULL, 0); } opt_param_list ')' opt_member_init_list block
        {
            /* Constructor: Class::Class(...) {}.
             *
             * This used to be written as an inline "qname_prefix TYPE_NAME
             * '(' ..." -- i.e. a second, separate grammar rule with the
             * exact same right-hand side as qualified_type's own
             * production (qname_prefix TYPE_NAME), just followed by more
             * symbols. That turned out to be a real bug, not a benign
             * conflict: bison's generated parser rejected '(' after
             * "Counter::Counter" and only accepted a further "::",
             * confirmed against an actual failing build (see the
             * postmortem in docs/DESIGN_NOTES.md -- this is exactly the
             * kind of thing that needs to be verified against real bison
             * output, not just pattern-matched against a known-benign
             * conflict family, which is the mistake that let this ship in
             * the first place).
             *
             * Routing through qualified_type instead means there's only
             * ONE grammar path that ever reduces "qname_prefix TYPE_NAME",
             * and the only fork left is "qualified_type used as a return
             * type" (type_spec's existing alternative) vs "qualified_type
             * followed directly by '('" (this rule) -- structurally
             * identical to the var_decl-vs-func_decl/func_def fork this
             * grammar already relies on everywhere else, and which is
             * actually confirmed working (every existing test file
             * exercises it).
             *
             * $1 (qualified_type) bundles the WHOLE dotted name as a flat
             * list -- e.g. [Counter, Counter] for `Counter::Counter`, or
             * [v32, Timer, Timer] for a hypothetical nested case -- so the
             * constructor's own name is $1's LAST component, and the
             * qualifier chain (what the other two out_of_line_def
             * alternatives store in `b`) is everything before it.
             */
            AstList *parts = &$1->list;
            int n = parts->count;
            $$ = ast_new(AST_FUNC_DEF, @1.first_line);
            $$->str1 = strdup(parts->items[n - 1]->str1);
            $$->type = NULL;
            $$->list = $4;
            $$->c = $6;    /* member-initializer list, or NULL -- see
                               opt_member_init_list's own doc comment */
            $$->a = $7;
            $$->b = ast_new(AST_QUALIFIED_ID, @1.first_line);
            for (int i = 0; i < n - 1; i++) {
                ast_list_append(&$$->b->list, parts->items[i]);
            }
            symtab_pop_scope(g_symtab);
        }
    | qname_prefix '~' TYPE_NAME '(' ')' block
        {
            /* Destructor: Class::~Class() {} -- never takes parameters, so
             * no function-scope push/pop is needed here (unlike the two
             * alternatives above). */
            $$ = ast_new(AST_FUNC_DEF, @1.first_line);
            size_t len = strlen($3) + 2;
            char *dtor_name = malloc(len);
            snprintf(dtor_name, len, "~%s", $3);
            $$->str1 = dtor_name;
            $$->type = NULL;
            $$->list = ast_list_new();
            $$->a = $6;
            $$->b = ast_new(AST_QUALIFIED_ID, @1.first_line);
            $$->b->list = $1;
        }
    ;

opt_param_list:
      /* empty */   { $$ = ast_list_new(); }
    | param_list     { $$ = $1; }
    ;

param_list:
      param                    { $$ = ast_list_new(); ast_list_append(&$$, $1); }
    | param_list ',' param     { $$ = $1; ast_list_append(&$$, $3); }
    ;

param:
    type_spec pointer_opt IDENTIFIER
        {
            symtab_insert(g_symtab, g_symtab->current, $3, SYM_PARAM);
            $$ = ast_new(AST_PARAM, @3.first_line);
            $$->str1 = strdup($3);
            $$->type = ($2 == 1) ? ast_wrap_pointer($1, @1.first_line)
                     : ($2 == 2) ? ast_wrap_reference($1, @1.first_line)
                     : $1;
        }
    | type_spec pointer_opt IDENTIFIER '[' ']'
        {
            /* Array PARAMETER syntax, `void foo(int arr[])` -- matches
             * real C/C++'s own array-to-pointer decay: a parameter
             * declared this way is semantically IDENTICAL to a pointer
             * parameter with no size information preserved at all, so
             * it's represented as an ordinary AST_POINTER_TYPE directly,
             * not AST_ARRAY_TYPE (which owns and carries its own known
             * length -- a parameter never does). Nothing downstream
             * needs to know this parameter was ever spelled with
             * brackets at all; by the time sema.c or codegen.c sees it,
             * it's just a pointer, the same as `int *arr` would produce. */
            symtab_insert(g_symtab, g_symtab->current, $3, SYM_PARAM);
            $$ = ast_new(AST_PARAM, @3.first_line);
            $$->str1 = strdup($3);
            AstNode *base = ($2 == 1) ? ast_wrap_pointer($1, @1.first_line)
                          : ($2 == 2) ? ast_wrap_reference($1, @1.first_line)
                          : $1;
            $$->type = ast_wrap_pointer(base, @1.first_line);
        }
    | type_spec pointer_opt IDENTIFIER '[' INT_LITERAL ']'
        {
            /* Array parameter WITH a size written, `void foo(int
             * arr[8])` -- real C++ accepts and silently ignores the
             * size here too (it plays no role at all; the parameter is
             * still just a pointer), so this project does the same:
             * $5 (the size) is intentionally unused. Same decay
             * reasoning as the empty-bracket alternative immediately
             * above. */
            symtab_insert(g_symtab, g_symtab->current, $3, SYM_PARAM);
            $$ = ast_new(AST_PARAM, @3.first_line);
            $$->str1 = strdup($3);
            AstNode *base = ($2 == 1) ? ast_wrap_pointer($1, @1.first_line)
                          : ($2 == 2) ? ast_wrap_reference($1, @1.first_line)
                          : $1;
            $$->type = ast_wrap_pointer(base, @1.first_line);
        }
    ;

pointer_opt:
      /* empty */  { $$ = 0; }
    | '*'          { $$ = 1; }
    | '&'          { $$ = 2; }
    ;

/* ---- types ------------------------------------------------------------ */

type_spec:
      INT_KW        { $$ = ast_ident("int", @1.first_line); }
    | FLOAT_KW      { $$ = ast_ident("float", @1.first_line); }
    | VOID_KW       { $$ = ast_ident("void", @1.first_line); }
    | BOOL_KW       { $$ = ast_ident("bool", @1.first_line); }
    | CHAR_KW       { $$ = ast_ident("char", @1.first_line); }
    | TYPE_NAME     { $$ = ast_ident($1, @1.first_line); }
    | qualified_type { $$ = $1; }
    ;

/* Qualified names (v32::Timer, Outer::Inner, ...). The lexer has already
 * resolved TYPE_NAME-vs-IDENTIFIER classification for every component by
 * the time these tokens reach the parser (see lexer.l) -- these rules just
 * assemble the AST, no symbol-table calls needed here. */

name_tok:
      IDENTIFIER  { $$ = $1; }
    | TYPE_NAME   { $$ = $1; }
    ;

qname_prefix:
      name_tok COLONCOLON
        {
            $$ = ast_list_new();
            ast_list_append(&$$, ast_ident($1, @1.first_line));
        }
    | qname_prefix name_tok COLONCOLON
        {
            $$ = $1;
            ast_list_append(&$$, ast_ident($2, @2.first_line));
        }
    ;

qualified_type:
    qname_prefix TYPE_NAME
        {
            $$ = ast_new(AST_QUALIFIED_ID, @1.first_line);
            $$->list = $1;
            ast_list_append(&$$->list, ast_ident($2, @2.first_line));
        }
    ;

qualified_id_expr:
    qname_prefix IDENTIFIER
        {
            $$ = ast_new(AST_QUALIFIED_ID, @1.first_line);
            $$->list = $1;
            ast_list_append(&$$->list, ast_ident($2, @2.first_line));
        }
    ;

/* ---- declarations ------------------------------------------------------ */

var_decl:
    type_spec pointer_opt IDENTIFIER opt_initializer
        {
            symtab_insert(g_symtab, g_symtab->current, $3, SYM_VAR);
            $$ = ast_new(AST_VAR_DECL, @3.first_line);
            $$->str1 = strdup($3);
            $$->type = ($2 == 1) ? ast_wrap_pointer($1, @1.first_line)
                     : ($2 == 2) ? ast_wrap_reference($1, @1.first_line)
                     : $1;
            $$->a = $4;
        }
    | type_spec pointer_opt IDENTIFIER '[' INT_LITERAL ']' opt_array_initializer
        {
            /* Standard C/C++ array declarator: length AFTER the name --
             * `int scores[8];`, optionally `= {1, 2, 3};` alongside it. */
            symtab_insert(g_symtab, g_symtab->current, $3, SYM_VAR);
            $$ = ast_new(AST_VAR_DECL, @3.first_line);
            $$->str1 = strdup($3);
            AstNode *base = ($2 == 1) ? ast_wrap_pointer($1, @1.first_line)
                          : ($2 == 2) ? ast_wrap_reference($1, @1.first_line)
                          : $1;
            $$->type = ast_wrap_array(base, $5, @1.first_line);
            $$->a = $7;
        }
    | type_spec '[' INT_LITERAL ']' IDENTIFIER opt_array_initializer
        {
            /* Vircon32-native-style array declarator, accepted as an
             * ALTERNATE valid C++-side input form -- same meaning as
             * the standard-C alternative above, length BEFORE the name
             * instead of after (`int [8] scores;`), matching Vircon32 C
             * itself. Deliberately no pointer_opt here (unlike the
             * standard-C form) -- this form exists specifically to let
             * someone already fluent in Vircon32 C, or transitioning
             * from it, keep writing what's already familiar to them
             * without having to also learn a second, unrelated
             * declarator convention; it isn't trying to be a general
             * C-declarator sublanguage of its own. Both forms produce
             * an identical AST_ARRAY_TYPE -- codegen always emits
             * Vircon32's own required form regardless of which one the
             * source used, so this choice is purely a source-reading
             * preference, never a behavioral one. */
            symtab_insert(g_symtab, g_symtab->current, $5, SYM_VAR);
            $$ = ast_new(AST_VAR_DECL, @5.first_line);
            $$->str1 = strdup($5);
            $$->type = ast_wrap_array($1, $3, @1.first_line);
            $$->a = $6;
        }
    ;

opt_array_initializer:
      /* empty */                     { $$ = NULL; }
    | '=' '{' opt_arg_list '}'         {
            /* `= {1, 2, 3}` (or `= {}`, an empty list -- accepted
             * syntactically, same reasoning as opt_arg_list's own empty
             * case for an ordinary call). No length-checking against the
             * array's own declared size happens anywhere yet (neither
             * "too many initializers" nor padding a short list with
             * zeros) -- sema.c doesn't currently look at this node at
             * all beyond ordinary expression recursion. A real,
             * documented gap, not silently handled. */
            $$ = ast_new(AST_INIT_LIST, @1.first_line);
            $$->list = $3;
        }
    ;



opt_initializer:
      /* empty */    { $$ = NULL; }
    | '=' expr        { $$ = $2; }
    ;

typedef_decl:
    TYPEDEF type_spec pointer_opt IDENTIFIER
        {
            symtab_insert(g_symtab, g_symtab->current, $4, SYM_TYPEDEF);
            $$ = ast_new(AST_TYPEDEF_DECL, @4.first_line);
            $$->str1 = strdup($4);
            $$->type = ($3 == 1) ? ast_wrap_pointer($2, @2.first_line)
                     : ($3 == 2) ? ast_wrap_reference($2, @2.first_line)
                     : $2;
        }
    ;

/* ---- statements --------------------------------------------------------- */

block:
    '{' { symtab_push_scope(g_symtab, NULL, 0); } stmt_list '}'
        {
            symtab_pop_scope(g_symtab);
            $$ = ast_new(AST_BLOCK, @1.first_line);
            $$->list = $3;
        }
    ;

stmt_list:
      /* empty */        { $$ = ast_list_new(); }
    | stmt_list stmt      { $$ = $1; ast_list_append(&$$, $2); }
    ;

stmt:
      block                              { $$ = $1; }
    | IF '(' expr ')' stmt  %prec LOWER_THAN_ELSE
        {
            $$ = ast_new(AST_IF, @1.first_line);
            $$->a = $3; $$->b = $5; $$->c = NULL;
        }
    | IF '(' expr ')' stmt ELSE stmt
        {
            $$ = ast_new(AST_IF, @1.first_line);
            $$->a = $3; $$->b = $5; $$->c = $7;
        }
    | WHILE '(' expr ')' stmt
        {
            $$ = ast_new(AST_WHILE, @1.first_line);
            $$->a = $3; $$->b = $5;
        }
    | SWITCH '(' expr ')' '{' switch_body '}'
        {
            /* Real C's own fall-through switch, passed through to
             * codegen.c essentially unchanged -- see AST_SWITCH's own
             * doc comment in ast.h for why no lowering transformation
             * happens here at all (Vircon32 C already has native
             * switch/case). switch_body builds one FLAT list mixing
             * CASE/DEFAULT labels with ordinary statements, in source
             * order -- not a list of separate per-case containers --
             * exactly matching real C's own grammar shape (a case/
             * default is a LABEL on the statement that follows it, not
             * a container), which is what makes fall-through "just
             * happen" rather than needing to be specially implemented
             * anywhere in this pipeline. */
            $$ = ast_new(AST_SWITCH, @1.first_line);
            $$->a = $3;
            $$->list = $6;
        }
    | FOR '(' { symtab_push_scope(g_symtab, NULL, 0); } for_init ';' expr_opt ';' expr_opt ')' stmt
        {
            /* Own scope so a loop-local `int i` in for_init doesn't leak
             * into the enclosing block/function (and so a second, later
             * `for (int i ...)` in the same function doesn't collide with
             * it in the symbol table). */
            symtab_pop_scope(g_symtab);
            $$ = ast_new(AST_FOR, @1.first_line);
            $$->a = $4; $$->b = $6; $$->c = $8; $$->d = $10;
        }
    | RETURN expr_opt ';'
        {
            $$ = ast_new(AST_RETURN, @1.first_line);
            $$->a = $2;
        }
    | BREAK ';'
        { $$ = ast_new(AST_BREAK, @1.first_line); }
    | CONTINUE ';'
        { $$ = ast_new(AST_CONTINUE, @1.first_line); }
    | var_decl ';'      { $$ = $1; }
    | typedef_decl ';'  { $$ = $1; }
    | expr ';'
        {
            $$ = ast_new(AST_EXPR_STMT, @1.first_line);
            $$->a = $1;
        }
    | ';'
        {
            $$ = ast_new(AST_EXPR_STMT, @1.first_line);
        }
    ;

for_init:
      /* empty */  { $$ = NULL; }
    | var_decl      { $$ = $1; }
    | expr
        {
            $$ = ast_new(AST_EXPR_STMT, @1.first_line);
            $$->a = $1;
        }
    ;

expr_opt:
      /* empty */  { $$ = NULL; }
    | expr          { $$ = $1; }
    ;

/* ---- switch bodies -------------------------------------------------------
 *
 * A flat, source-ordered list -- CASE/DEFAULT entries are LABELS
 * appended directly alongside ordinary statements, never containers of
 * their own. This is deliberately the SAME shape real C's own grammar
 * gives a switch body (a labeled-statement is just a statement with a
 * label attached, not a special container), which is exactly what
 * makes fall-through behavior fall out for free -- nothing about
 * "don't insert an implicit break" needs to be implemented anywhere;
 * the list is simply walked in order, the same way an AST_BLOCK's own
 * list already is.
 */
switch_body:
      /* empty */                    { $$ = ast_list_new(); }
    | switch_body CASE expr ':'
        {
            $$ = $1;
            AstNode *c = ast_new(AST_CASE, @2.first_line);
            c->a = $3;
            ast_list_append(&$$, c);
        }
    | switch_body DEFAULT ':'
        {
            $$ = $1;
            AstNode *d = ast_new(AST_DEFAULT, @2.first_line);
            ast_list_append(&$$, d);
        }
    | switch_body stmt
        { $$ = $1; ast_list_append(&$$, $2); }
    ;

/* ---- expressions ---------------------------------------------------------
 *
 * Layered primary/postfix/unary/expr grammar (the standard C-grammar
 * idiom): postfix operators (call, member access, subscript, postfix ++/--)
 * bind tighter than prefix unary operators, which bind tighter than the
 * binary operators in `expr`. Because each layer is its own nonterminal,
 * this structural precedence falls out of the grammar shape itself -- the
 * %left/%right declarations above are only needed to resolve the genuine
 * ambiguity in the flat `expr op expr` alternation below (operator
 * precedence/associativity among binary operators), not for the unary
 * layer.
 */

primary_expr:
      IDENTIFIER        { $$ = ast_ident($1, @1.first_line); }
    | INT_LITERAL         { $$ = ast_new(AST_INT_LIT, @1.first_line); $$->ival = $1; }
    | FLOAT_LITERAL        { $$ = ast_new(AST_FLOAT_LIT, @1.first_line); $$->fval = $1; }
    | STRING_LITERAL        { $$ = ast_new(AST_STRING_LIT, @1.first_line); $$->str1 = $1; }
    | CHAR_LITERAL            { $$ = ast_new(AST_CHAR_LIT, @1.first_line); $$->ival = $1; }
    | TRUE_KW                  { $$ = ast_new(AST_BOOL_LIT, @1.first_line); $$->ival = 1; }
    | FALSE_KW                  { $$ = ast_new(AST_BOOL_LIT, @1.first_line); $$->ival = 0; }
    | THIS                        { $$ = ast_new(AST_THIS, @1.first_line); }
    | qualified_id_expr             { $$ = $1; }
    | '(' expr ')'                    { $$ = $2; }
    ;

postfix_expr:
      primary_expr                          { $$ = $1; }
    | postfix_expr '(' opt_arg_list ')'
        {
            $$ = ast_new(AST_CALL, @1.first_line);
            $$->a = $1;
            $$->list = $3;
        }
    | postfix_expr '.' IDENTIFIER
        {
            $$ = ast_new(AST_MEMBER, @1.first_line);
            $$->str1 = strdup(".");
            $$->str2 = strdup($3);
            $$->a = $1;
        }
    | postfix_expr ARROW IDENTIFIER
        {
            $$ = ast_new(AST_MEMBER, @1.first_line);
            $$->str1 = strdup("->");
            $$->str2 = strdup($3);
            $$->a = $1;
        }
    | postfix_expr '[' expr ']'
        {
            $$ = ast_new(AST_SUBSCRIPT, @1.first_line);
            $$->a = $1;
            $$->b = $3;
        }
    | postfix_expr INC
        {
            $$ = ast_new(AST_UNOP, @1.first_line);
            $$->str1 = strdup("post++");
            $$->a = $1;
        }
    | postfix_expr DEC
        {
            $$ = ast_new(AST_UNOP, @1.first_line);
            $$->str1 = strdup("post--");
            $$->a = $1;
        }
    ;

unary_expr:
      postfix_expr           { $$ = $1; }
    | '!' unary_expr
        { $$ = ast_new(AST_UNOP, @1.first_line); $$->str1 = strdup("!"); $$->a = $2; }
    | '~' unary_expr
        { $$ = ast_new(AST_UNOP, @1.first_line); $$->str1 = strdup("~"); $$->a = $2; }
    | '-' unary_expr
        { $$ = ast_new(AST_UNOP, @1.first_line); $$->str1 = strdup("neg"); $$->a = $2; }
    | '&' unary_expr
        { $$ = ast_new(AST_UNOP, @1.first_line); $$->str1 = strdup("addr"); $$->a = $2; }
    | '*' unary_expr
        { $$ = ast_new(AST_UNOP, @1.first_line); $$->str1 = strdup("deref"); $$->a = $2; }
    | INC unary_expr
        { $$ = ast_new(AST_UNOP, @1.first_line); $$->str1 = strdup("pre++"); $$->a = $2; }
    | DEC unary_expr
        { $$ = ast_new(AST_UNOP, @1.first_line); $$->str1 = strdup("pre--"); $$->a = $2; }
    | NEW type_spec
        { $$ = ast_new(AST_NEW, @1.first_line); $$->type = $2; }
    | NEW type_spec '(' opt_arg_list ')'
        { $$ = ast_new(AST_NEW, @1.first_line); $$->type = $2; $$->list = $4; }
    | NEW type_spec '[' expr ']'
        {
            /* new T[N] -- heap-allocated array. N is any runtime
             * expression (not restricted to a compile-time constant the
             * way a stack array's own declared length is), held
             * directly as an expression node in `a`. Constructor
             * arguments alongside an array size (`new T[N](args)`)
             * aren't accepted by this grammar at all -- see ast.h's own
             * doc comment on AST_NEW for why, and lower.c/codegen.c for
             * how this lowers (allocation only, no per-element
             * construction yet). */
            $$ = ast_new(AST_NEW, @1.first_line);
            $$->type = $2;
            $$->a = $4;
        }
    | DELETE unary_expr
        { $$ = ast_new(AST_DELETE, @1.first_line); $$->a = $2; }
    | DELETE '[' ']' unary_expr
        {
            /* delete[] ptr -- see ast.h's own doc comment on AST_DELETE
             * for why this currently lowers identically to plain
             * `delete` (no per-element destructor invocation exists for
             * either new[] or delete[] yet); the distinction is
             * recorded (ival=1) but not yet acted on anywhere. */
            $$ = ast_new(AST_DELETE, @1.first_line);
            $$->a = $4;
            $$->ival = 1;
        }
    | '(' type_spec pointer_opt ')' unary_expr
        {
            /* C-style cast -- (Type)expr, (Type *)expr, (Type &)expr.
             * AST_CAST already existed (lower.c has been synthesizing
             * one internally for a while -- implicit-upcast insertion,
             * receiver casts -- see ast.h's own doc comment on it,
             * updated here now that user-written source can produce
             * one too, not just lowering).
             *
             * No genuine ambiguity with primary_expr's own
             * "'(' expr ')'" parenthesized-expression alternative,
             * confirmed directly from primary_expr's own grammar
             * before writing this, not assumed: primary_expr only ever
             * accepts a bare IDENTIFIER as an expression-starting
             * token, never TYPE_NAME -- so a TYPE_NAME (or a built-in
             * type keyword: INT_KW/FLOAT_KW/VOID_KW/BOOL_KW/CHAR_KW, all
             * of which start type_spec and nothing in expr's own
             * first-set) immediately after '(' can ONLY mean a cast is
             * starting here, never the start of a plain parenthesized
             * expression. */
            $$ = ast_new(AST_CAST, @1.first_line);
            $$->type = ($3 == 1) ? ast_wrap_pointer($2, @1.first_line)
                     : ($3 == 2) ? ast_wrap_reference($2, @1.first_line)
                     : $2;
            $$->a = $5;
        }
    | cpp_cast_kw '<' type_spec pointer_opt '>' '(' expr ')'
        {
            /* C++-style cast -- static_cast<T>(x), const_cast<T>(x),
             * reinterpret_cast<T>(x), dynamic_cast<T>(x). All four
             * build the exact same AST_CAST node the C-style cast above
             * does -- see AST_CAST's own doc comment in ast.h for why
             * real C++'s distinctions between them collapse to nothing
             * once the target is C, which has no notion of any of
             * these cast KINDS, only a single generic cast syntax.
             * dynamic_cast is marked (ival=1) so sema.c's own
             * check_node can warn about the one real, substantive gap
             * this collapsing introduces: no actual RTTI-backed runtime
             * check happens, unlike what real dynamic_cast promises.
             *
             * The '<' '>' here are the same tokens comparison operators
             * already use, not new ones -- no ambiguity in practice,
             * since they only ever appear in THIS shape immediately
             * after one of the four cast keywords, a grammar position
             * comparison never occurs in. This is NOT template syntax
             * and doesn't open the door to one -- it's a fixed, four-
             * keyword special form, not a general
             * "identifier < args >" production the way an actual
             * template instantiation would need. */
            $$ = ast_new(AST_CAST, @1.first_line);
            $$->type = ($4 == 1) ? ast_wrap_pointer($3, @1.first_line)
                     : ($4 == 2) ? ast_wrap_reference($3, @1.first_line)
                     : $3;
            $$->a = $7;
            $$->ival = ($1 == 3) ? 1 : 0; /* 1 only for dynamic_cast */
        }
    ;

/* Distinguishes which of the four C++-style cast keywords introduced
 * this cast -- see the unary_expr production above for how the value
 * is used. Grouped as one shared production (matching class_or_struct_kw's
 * own established pattern) rather than four separate, nearly-identical
 * unary_expr alternatives, since three of the four are semantically
 * IDENTICAL here (see AST_CAST's own doc comment in ast.h) and the
 * fourth (dynamic_cast) differs only in setting one extra flag
 * afterward, not in any part of the grammar shape itself. */
cpp_cast_kw:
      STATIC_CAST        { $$ = 0; }
    | CONST_CAST          { $$ = 1; }
    | REINTERPRET_CAST     { $$ = 2; }
    | DYNAMIC_CAST          { $$ = 3; }
    ;

expr:
      unary_expr           { $$ = $1; }
    | expr '*' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("*"); $$->a = $1; $$->b = $3; }
    | expr '/' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("/"); $$->a = $1; $$->b = $3; }
    | expr '%' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("%"); $$->a = $1; $$->b = $3; }
    | expr '+' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("+"); $$->a = $1; $$->b = $3; }
    | expr '-' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("-"); $$->a = $1; $$->b = $3; }
    | expr '<' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("<"); $$->a = $1; $$->b = $3; }
    | expr '>' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup(">"); $$->a = $1; $$->b = $3; }
    | expr LE expr     { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("<="); $$->a = $1; $$->b = $3; }
    | expr GE expr     { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup(">="); $$->a = $1; $$->b = $3; }
    | expr EQ expr     { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("=="); $$->a = $1; $$->b = $3; }
    | expr NE expr     { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("!="); $$->a = $1; $$->b = $3; }
    | expr ANDAND expr  { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("&&"); $$->a = $1; $$->b = $3; }
    | expr OROR expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("||"); $$->a = $1; $$->b = $3; }
    | expr '&' expr
        {
            /* Binary bitwise-AND -- coexists with unary_expr's own
             * "'&' unary_expr" (address-of) the exact same way binary
             * '-' already coexists with unary_expr's own "'-' unary_expr"
             * (negation): the two never conflict, since unary_expr is a
             * different grammar POSITION (a prefix, at the start of an
             * operand) than this rule's own infix use (between two
             * already-reduced expr's) -- the same proven disambiguation
             * this grammar already relies on, not a new risk. */
            $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("&"); $$->a = $1; $$->b = $3;
        }
    | expr '|' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("|"); $$->a = $1; $$->b = $3; }
    | expr '^' expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("^"); $$->a = $1; $$->b = $3; }
    | expr SHL expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup("<<"); $$->a = $1; $$->b = $3; }
    | expr SHR expr    { $$ = ast_new(AST_BINOP, @1.first_line); $$->str1 = strdup(">>"); $$->a = $1; $$->b = $3; }
    | expr '=' expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("="); $$->a = $1; $$->b = $3; }
    | expr PLUSEQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("+="); $$->a = $1; $$->b = $3; }
    | expr MINUSEQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("-="); $$->a = $1; $$->b = $3; }
    | expr STAREQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("*="); $$->a = $1; $$->b = $3; }
    | expr SLASHEQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("/="); $$->a = $1; $$->b = $3; }
    | expr ANDEQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("&="); $$->a = $1; $$->b = $3; }
    | expr OREQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("|="); $$->a = $1; $$->b = $3; }
    | expr XOREQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("^="); $$->a = $1; $$->b = $3; }
    | expr SHLEQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup("<<="); $$->a = $1; $$->b = $3; }
    | expr SHREQ expr
        { $$ = ast_new(AST_ASSIGN, @1.first_line); $$->str1 = strdup(">>="); $$->a = $1; $$->b = $3; }
    ;

opt_arg_list:
      /* empty */  { $$ = ast_list_new(); }
    | arg_list      { $$ = $1; }
    ;

arg_list:
      expr                  { $$ = ast_list_new(); ast_list_append(&$$, $1); }
    | arg_list ',' expr      { $$ = $1; ast_list_append(&$$, $3); }
    ;

/* ---- member-initializer lists: `Derived::Derived(args) : Base(base_args)
 * { ... }` -- base-class constructor delegation. Grammatically also
 * accepts `: field(val)` (an ORDINARY member field's own name, lexed as
 * IDENTIFIER rather than TYPE_NAME -- the same distinction this grammar
 * already relies on everywhere else to tell a registered class/type name
 * apart from an ordinary identifier), even though this round only ACTS on
 * the base-class-delegation case -- sema.c's resolve_member_init_list
 * reports the member-field case as a clear, explicit "not yet supported"
 * error, rather than have the grammar reject valid C++ syntax outright
 * with a confusing parse failure instead of a clear semantic diagnostic.
 *
 * opt_member_init_list produces NULL (not an empty list) when no ": ..."
 * was written at all -- callers (func_def, out_of_line_def's constructor
 * alternative) store this directly in AST_FUNC_DEF's own `c` slot, so
 * `c == NULL` is exactly "no member-initializer list was written",
 * matching how `b` on AST_FUNC_DEF is NULL for an in-class definition. */
opt_member_init_list:
      /* empty */                    { $$ = NULL; }
    | ':' member_init_list            { $$ = ast_new(AST_MEMBER_INIT_LIST, @1.first_line); $$->list = $2; }
    ;

member_init_list:
      member_init                          { $$ = ast_list_new(); ast_list_append(&$$, $1); }
    | member_init_list ',' member_init      { $$ = $1; ast_list_append(&$$, $3); }
    ;

member_init:
      IDENTIFIER '(' opt_arg_list ')'
        {
            /* An ordinary member field's own name -- see this section's
             * own header comment above for why this is accepted
             * syntactically despite not being acted on yet. */
            $$ = ast_new(AST_MEMBER_INIT, @1.first_line);
            $$->str1 = strdup($1);
            $$->list = $3;
        }
    | TYPE_NAME '(' opt_arg_list ')'
        {
            /* A registered class name -- the base-class-delegation case
             * this round actually implements. Whether $1 is genuinely
             * THIS constructor's own direct base (as opposed to some
             * other, unrelated class name that merely happens to be
             * registered) is sema.c's job, not the parser's -- same
             * division of labor as everywhere else in this grammar. */
            $$ = ast_new(AST_MEMBER_INIT, @1.first_line);
            $$->str1 = strdup($1);
            $$->list = $3;
        }
    ;

%%

void yyerror(const char *msg) {
    fprintf(stderr, "%s:%d: error: %s\n", g_current_filename, g_lex_lineno, msg);
}
