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
 * parse") later without a rewrite. As written, this grammar has no real
 * conflicts -- the typedef/class-name-vs-value disambiguation is handled
 * by the lexer (see lexer.l and symtab.h), which is what keeps `Foo * x;`
 * unambiguous without needing the parser to fork.
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
/* %error-verbose is the old (but still supported) spelling of what newer
 * bison calls `%define parse.error verbose`. Using the old spelling keeps
 * this grammar buildable on both bison 2.3 and current bison; you'll get
 * a harmless "deprecated directive" warning on newer bison, nothing more. */

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

%token CLASS PUBLIC PRIVATE PROTECTED NAMESPACE TYPEDEF
%token RETURN IF ELSE WHILE FOR
%token INT_KW FLOAT_KW VOID_KW BOOL_KW CHAR_KW
%token NEW DELETE THIS VIRTUAL TRUE_KW FALSE_KW
%token COLONCOLON ARROW EQ NE LE GE ANDAND OROR
%token PLUSEQ MINUSEQ STAREQ SLASHEQ INC DEC

%type <node> program top_decl namespace_decl class_decl member
%type <node> func_decl func_def func_header var_decl typedef_decl
%type <node> block stmt for_init opt_initializer
%type <node> expr expr_opt unary_expr postfix_expr primary_expr
%type <node> qualified_id_expr qualified_type type_spec param

%type <list> top_decl_list member_list stmt_list
%type <list> param_list opt_param_list arg_list opt_arg_list qname_prefix

%type <str> opt_base name_tok
%type <access> access_spec
%type <ival> pointer_opt opt_virtual

%right '=' PLUSEQ MINUSEQ STAREQ SLASHEQ
%left OROR
%left ANDAND
%left EQ NE
%left '<' '>' LE GE
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
    CLASS IDENTIFIER opt_base
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
        }
    '{' member_list '}'
        {
            symtab_pop_scope(g_symtab);
            $$ = ast_new(AST_CLASS_DECL, @1.first_line);
            $$->str1 = strdup($2);
            $$->str2 = $3 ? strdup($3) : NULL;
            $$->list = $6;
            g_current_class_sym = NULL;
        }
    ;

opt_base:
      /* empty */                { $$ = NULL; }
    | ':' PUBLIC TYPE_NAME        { $$ = $3; }
    | ':' PRIVATE TYPE_NAME       { $$ = $3; }
    /* TODO: the inheritance access-specifier (public/private above) isn't
     * carried onto the AST yet -- $$ only keeps the base class name.
     * Stash it (e.g. reuse AstNode.access) once the lowering pass needs
     * to know whether inheritance is public or private. */
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

func_header:
      type_spec IDENTIFIER '(' { symtab_push_scope(g_symtab, NULL, 0); } opt_param_list ')'
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
            /* TODO: stash $1 (virtual-ness) on $$ once the vtable-lowering
             * pass needs it -- e.g. repurpose AstNode.ival as a flags
             * field. */
            symtab_pop_scope(g_symtab); /* prototype only; no body needs the param scope */
        }
    ;

func_def:
    opt_virtual func_header block
        {
            $$ = $2;
            $$->kind = AST_FUNC_DEF;
            $$->a = $3;
            symtab_pop_scope(g_symtab);
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
    | DELETE unary_expr
        { $$ = ast_new(AST_DELETE, @1.first_line); $$->a = $2; }
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
    ;

opt_arg_list:
      /* empty */  { $$ = ast_list_new(); }
    | arg_list      { $$ = $1; }
    ;

arg_list:
      expr                  { $$ = ast_list_new(); ast_list_append(&$$, $1); }
    | arg_list ',' expr      { $$ = $1; ast_list_append(&$$, $3); }
    ;

%%

void yyerror(const char *msg) {
    fprintf(stderr, "%s:%d: error: %s\n", g_current_filename, g_lex_lineno, msg);
}
