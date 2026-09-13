#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "ast.h"

/*
 * codegen.c -- emits Vircon32 C source text from the fully-lowered AST.
 *
 * PRECONDITION: same as lower_run()'s -- sema_run() must have completed
 * with zero errors, and lower_run() must have already run. This module
 * does no validation of its own; it trusts every ClassLayout (sema.c)
 * and StructLayout (lower.c) it reads to be complete and correct.
 *
 * TWO VIRCON32-SPECIFIC QUIRKS THIS MODULE MUST RESPECT (neither is
 * standard C, and getting either wrong produces code that won't compile
 * on the actual target, not just code that looks slightly off):
 *
 *   1. Every `struct Name { ... };` DEFINITION is auto-typedef'd by the
 *      Vircon32 C compiler under its own tag name -- every SUBSEQUENT
 *      REFERENCE to that type (a pointer field, a parameter, a return
 *      type, anywhere) must be the BARE name (`Name *next;`), never
 *      prefixed with `struct` (`struct Name *next;` is a Vircon32
 *      compile ERROR, not merely redundant). Definitions themselves
 *      still use the `struct Name { ... };` form -- only references drop
 *      the keyword.
 *
 *   2. Array declarators put the length in brackets BEFORE the variable
 *      name (`int [8] myarray;`), not after it the way standard C does
 *      (`int myarray[8];`). USE, once declared, is ordinary subscript
 *      syntax (`myarray[2] = myarray[2] + 7;`) -- only the DECLARATION
 *      form is reversed. NOT YET RELEVANT: this project's grammar has no
 *      array-type declarator at all yet (AST_SUBSCRIPT exists as an
 *      expression -- `a[i]` -- but nothing in var_decl/param/typedef
 *      grammar can produce an array TYPE), so there's nothing for this
 *      module to emit an array declaration for today. Recorded here so
 *      it isn't lost: whoever adds array-type support to the grammar
 *      needs to know print_type() (in codegen.c) has to special-case an
 *      array type's declarator by printing the bracketed length BEFORE
 *      the name, unlike every other type this module currently handles
 *      (which are all printed independently of the name being declared).
 *
 * DELIBERATELY NOT YET IN SCOPE (this is the first codegen round, not
 * the whole thing): method/function BODY emission, and vtable STATIC
 * INSTANCE emission (the vtable struct TYPE is emitted -- something has
 * to exist for a class's own `vtable` field to point at -- but no actual
 * static instance of that type, populated with real function pointers,
 * is generated yet). Both are substantial enough to deserve their own
 * round, verified against real output the same way every other phase in
 * this project has been.
 *
 * AN OPEN QUESTION, FLAGGED RATHER THAN GUESSED AT: this project's
 * front end doesn't require a class to be fully declared, textually,
 * before some OTHER class references it as a pointer member (every
 * class gets registered in one pass, via collect_declarations, before
 * any type resolution happens) -- so nothing stops a class earlier in
 * source order from holding a pointer to a class declared later. This
 * module emits struct definitions in natural source-encounter order,
 * which happens to work for every existing test file, but doesn't
 * resolve the general case. Whether Vircon32 C supports a standalone
 * forward declaration (`struct Foo;`, no body) the way standard C does
 * is genuinely unknown here -- needs confirming against the real
 * compiler/docs before this ordering gap can be called solved.
 */

/* Emits generated Vircon32 C source for the whole program to `out`
 * (already open for writing). Currently emits: top-level typedefs, each
 * class's vtable struct type (if it has any virtual methods), and each
 * class's own struct definition -- see the scope note above for what's
 * deliberately not here yet. */
void codegen_run(const AstNode *program, FILE *out);

#endif /* CODEGEN_H */
