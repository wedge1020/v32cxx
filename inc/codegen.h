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
 *   1. `struct` is ONLY valid syntax in the two DECLARATION forms
 *      themselves -- a forward declaration (`struct Name;`) or a full
 *      definition (`struct Name { ... };`). It is NEVER valid as a
 *      type-USE expression anywhere else -- not a pointer field
 *      (`struct Name *next;` errors), not even an ordinary variable
 *      declaration (`struct Name n;` errors too, "expected '{'", as if
 *      the parser were expecting ANOTHER declaration to follow). Every
 *      other reference to the type must be the bare name, and this is
 *      true starting from the FIRST forward declaration, not just once
 *      the full definition has appeared -- confirmed by testing against
 *      the real compiler (see docs/DESIGN_NOTES.md's "forward-reference
 *      ordering" section for the full progression of what did and
 *      didn't compile). `print_type()` is the one function every other
 *      part of this module funnels type output through, specifically so
 *      this rule only has to be gotten right in one place.
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
 * A THIRD QUIRK, discovered while confirming the above, not yet acted on
 * by any codegen phase but worth remembering for whenever one generates
 * pointer-initializing code: Vircon32 C rejects assigning the literal
 * `0` to a pointer ("cannot assign int to ... pointer"); it requires
 * `NULL` specifically. Nothing this module currently emits assigns a
 * pointer at all, so this hasn't mattered yet -- but the first codegen
 * phase that DOES generate a null-pointer initializer needs to emit the
 * literal text `NULL`, never a bare `0`.
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
 * THE FORWARD-REFERENCE ORDERING QUESTION FROM AN EARLIER ROUND IS NOW
 * RESOLVED: confirmed against the real compiler that a standalone
 * forward declaration (`struct Name;`, no body) is valid on its own,
 * doesn't conflict with a later full definition of the same tag, and is
 * sufficient to make the bare name usable as an (incomplete) pointer
 * type immediately, even before the full definition appears. codegen_run
 * now emits one for every class before anything else, which resolves the
 * general case completely -- two classes holding pointers to each other,
 * or simply one preceding another it points to, no longer depends on
 * source order happening to already be safe.
 */

/* Emits generated Vircon32 C source for the whole program to `out`
 * (already open for writing). Currently emits: a forward declaration for
 * every class, top-level typedefs, each class's vtable struct type (if
 * it has any virtual methods), and each class's own struct definition --
 * see the scope note above for what's deliberately not here yet. */
void codegen_run(const AstNode *program, FILE *out);

#endif /* CODEGEN_H */
