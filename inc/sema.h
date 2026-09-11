#ifndef SEMA_H
#define SEMA_H

#include "ast.h"

/*
 * First slice of semantic analysis. Deliberately narrow in scope -- see
 * the per-pass comments in sema.c and the README's "what's not here yet"
 * list for what this does NOT do (overload-aware matching, inherited-
 * member layout merging, access-control enforcement, vtable slot
 * assignment, full parameter-type-aware mangling).
 *
 * What it DOES do, run in this order by sema_run():
 *   1. Walk the whole program (recursing into namespaces) and register
 *      every class by its bare name into a flat registry.
 *   2. Attach out-of-line definitions (FUNC_DEF nodes produced by
 *      out_of_line_def in parser.y, identifiable by a non-NULL `b`
 *      qualifier chain) onto the matching in-class prototype, turning
 *      that prototype into the authoritative FUNC_DEF. The top-level
 *      duplicate is left in place but flagged (FuncSemaInfo.is_out_of_line)
 *      so a later codegen pass knows to skip re-emitting it.
 *   3. Compute a ClassLayout for every class: its data members and methods
 *      split out from AccessSpec markers, plus its resolved base class
 *      (by AST pointer, not just name).
 *   4. Assign every method (and every free function) a first-cut mangled
 *      name.
 */

typedef struct ClassLayout {
    AstList data_members;      /* AST_VAR_DECL nodes, in declaration order */
    AstList methods;           /* AST_FUNC_DECL/AST_FUNC_DEF nodes, in declaration order */
    AstNode *base_class_decl;  /* resolved AST_CLASS_DECL of the base class, or NULL.
                                 * NOTE: inherited members are NOT merged into
                                 * data_members/methods above -- a consumer that
                                 * needs "all members including inherited ones"
                                 * has to walk base_class_decl's own ClassLayout
                                 * (via its sema_info) explicitly. Not merged
                                 * automatically to avoid silently duplicating
                                 * members if layout computation ever runs more
                                 * than once over the same AST. */
} ClassLayout;

typedef struct FuncSemaInfo {
    char *mangled_name;   /* e.g. "Player__update" for a method, or just
                            * "clamp" for a free function -- see sema.c's
                            * mangle() for the scheme and its known gaps
                            * (no parameter-type disambiguation yet, so two
                            * overloads of the same name currently collide). */
    int is_out_of_line;   /* 1 on the top-level duplicate left behind by an
                            * out-of-line definition after its body has been
                            * moved onto the real class member -- codegen
                            * should skip emitting anything with this set. */
} FuncSemaInfo;

/*
 * Runs the pass over a fully-parsed Program AST. Returns the number of
 * semantic errors found (0 = clean); errors are reported to stderr with a
 * source line number, in the same spirit as yyerror.
 */
int sema_run(AstNode *program);

/* Prints a human-readable summary of every class's computed layout and
 * every function's mangled name -- useful for eyeballing that sema_run()
 * did what you expected, the same role ast_dump() plays for parsing. */
void sema_dump(const AstNode *program);

#endif /* SEMA_H */
