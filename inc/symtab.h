#ifndef SYMTAB_H
#define SYMTAB_H

/*
 * Scoped symbol table used for the classic "lexer hack": the lexer needs to
 * know, at scan time, whether an identifier names a type (a class or a
 * typedef) or an ordinary value, so it can hand the parser TYPE_NAME vs
 * IDENTIFIER tokens. That's what makes `Foo * x;` parseable as a pointer
 * declaration instead of a multiplication expression-statement.
 *
 * It also backs namespace-qualified lookup (`v32::Timer`, `ioports::WRITE`):
 * when the parser reduces a nested-name-specifier prefix (IDENTIFIER '::'),
 * it resolves that identifier to a namespace/class scope and tells the
 * symbol table "the next identifier token should be looked up inside THIS
 * scope, not the normal enclosing-scope chain." See symtab_set_pending_qualifier.
 *
 * NOTE ON GLR SAFETY: this table is mutated directly by semantic actions
 * (symtab_insert, symtab_push_scope/pop_scope). That's fine as long as the
 * region of grammar you're in is conflict-free, which it is for everything
 * in this skeleton grammar -- bison's GLR engine only forks parse stacks at
 * an actual shift/reduce or reduce/reduce conflict, and there are none here,
 * so actions run exactly once, same as plain LALR. If you extend the
 * declarator grammar later (function pointers, the "most vexing parse",
 * array-of-pointer-to-function syntax, etc.) and introduce real ambiguity,
 * audit any action in that region that mutates this table -- a speculative
 * GLR branch that gets discarded must not have already inserted symbols or
 * pushed/popped scopes as a side effect. The usual fix is to defer table
 * mutation out of the ambiguous production and into a later, unambiguous
 * reduction (e.g. only commit at the point a full declaration is complete).
 */

typedef enum {
    SYM_VAR,
    SYM_PARAM,
    SYM_FUNC,
    SYM_TYPEDEF,
    SYM_CLASS,
    SYM_ENUM,
    SYM_NAMESPACE
} SymbolKind;

typedef struct Symbol {
    char *name;             /* unqualified name as declared, e.g. "Timer" */
    char *qualified_name;   /* fully qualified, e.g. "v32::Timer" */
    SymbolKind kind;
    struct Scope *inner_scope; /* only for SYM_CLASS / SYM_NAMESPACE: their member scope */
    struct Symbol *next;       /* hash bucket chain */
} Symbol;

#include "v32cxx.h" /* SYMTAB_BUCKETS -- see v32cxx.h's own comment on it */

typedef struct Scope {
    Symbol *buckets[SYMTAB_BUCKETS];
    struct Scope *parent;   /* enclosing lexical scope; NULL at the global/TU scope */
    char *owner_name;       /* namespace/class name this scope belongs to, or NULL */
    int is_class_scope;     /* true for class/struct bodies */
} Scope;

typedef struct SymTab {
    Scope *global;      /* translation-unit scope, never popped */
    Scope *current;      /* innermost active scope during parsing */

    /*
     * Set by the parser immediately after reducing a nested-name-specifier
     * prefix ("Name ::"). Consulted by the *next* identifier-like token the
     * lexer scans, then cleared. NULL means "use normal enclosing-scope
     * lookup", which is the common case.
     */
    Scope *pending_qualifier;
} SymTab;

SymTab *symtab_create(void);
void symtab_destroy(SymTab *st);

/* Enter/leave a lexical scope (block, namespace body, class body). */
Scope *symtab_push_scope(SymTab *st, const char *owner_name, int is_class_scope);
void symtab_pop_scope(SymTab *st);

/* Declare `name` of kind `kind` in `scope`. Returns the new symbol
 * (or the existing one on redeclaration -- this skeleton doesn't diagnose
 * redefinition errors yet). */
Symbol *symtab_insert(SymTab *st, Scope *scope, const char *name, SymbolKind kind);

/* Normal C++-style lookup: search `current` scope, then its parent, etc. */
Symbol *symtab_lookup(SymTab *st, const char *name);

/* Lookup restricted to exactly one scope, no walking up to parents. */
Symbol *symtab_lookup_in(Scope *scope, const char *name);

/* Used by the lexer: is `name` currently visible as a class or typedef?
 * Honors pending_qualifier (and clears it after use) so that
 *   v32 :: Timer
 * looks "Timer" up inside v32's scope, not the enclosing scope chain. */
int symtab_is_type_name(SymTab *st, const char *name);

/* Same qualifier-aware behavior as symtab_is_type_name, but returns the
 * resolved Symbol* (or NULL) instead of a boolean. Used by the parser when
 * building qualified-id expressions/types. */
Symbol *symtab_lookup_qualified(SymTab *st, const char *name);

/*
 * Called by the LEXER -- not the parser -- the instant it recognizes a
 * COLONCOLON ("::") token, passing the Symbol* it resolved for the
 * identifier/type-name it just scanned immediately beforehand.
 *
 * WHY THE LEXER AND NOT THE PARSER: it's tempting to arm the qualifier from
 * a parser action after reducing "IDENTIFIER ::" to some ns_prefix
 * nonterminal. That's wrong: bison (LALR or GLR, either way, with the usual
 * 1-token lookahead) generally has to fetch the token *after* "::" before
 * it can even decide to perform that reduction, since distinguishing "one
 * more qualifier coming" from "this was the last component" needs to see
 * what follows. By the time the reduction's action runs, the lexer has
 * *already* scanned and classified that next token using whatever
 * qualifier context was active *before* -- one step too late.
 *
 * The lexer doesn't have this problem: it produces tokens strictly in scan
 * order, so it can update pending_qualifier the moment it emits COLONCOLON,
 * guaranteeing it's set before the *next* call into the scanner. See
 * lexer.l for the call site.
 *
 * If `sym` is NULL, or names something other than a namespace or class
 * (e.g. you wrote `someVariable::`, which isn't meaningful), this clears
 * pending_qualifier rather than leaving stale state armed; the parser will
 * then fail to find a sensible qualified_type/qualified_id production and
 * report a syntax error, which is an acceptable (if not maximally
 * friendly) diagnostic for this skeleton.
 */
void symtab_arm_qualifier(SymTab *st, Symbol *sym);

#endif /* SYMTAB_H */
