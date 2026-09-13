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
 * DELIBERATELY NOT YET IN SCOPE: vtable STATIC INSTANCE emission (the
 * vtable struct TYPE is emitted, and every method/function body now is
 * too -- but no actual static instance of a vtable type, populated with
 * real function pointers so a constructor could point a new object's
 * `vtable` field at it, is generated yet). Substantial enough to deserve
 * its own round, verified against real output the same way every other
 * phase in this project has been. (Method/function body emission,
 * previously listed here as not-yet-in-scope, is now done -- see below.)
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
 *
 * METHOD/FUNCTION BODY EMISSION is now implemented (print_expr/
 * print_stmt/emit_function_definition) -- every fully-lowered statement
 * and expression kind is handled, using an unconditional "wrap every
 * binary/assignment/prefix-unary expression in its own parens" strategy
 * rather than reconstructing C's precedence table (correctness over
 * cosmetics for a first pass). Prototype-only methods (a constructor/
 * destructor/method declared but never defined anywhere) are
 * deliberately NOT emitted at all -- there's genuinely no body to emit,
 * and this-injection never touches anything that isn't AST_FUNC_DEF, so
 * such a method's own parameter list wouldn't even have a receiver in
 * it. KNOWN LIMITATION: if one is ever actually called, the generated C
 * will fail to COMPILE (no prototype exists for the call to resolve
 * against), not merely fail to link.
 *
 * A REAL RISK FOUND BY TRACING THROUGH BY HAND -- NOW FIXED, CONFIRMED
 * ONLY BY REASONING, NOT YET BY A REAL COMPILE: a call to an INHERITED
 * method (virtual or not) used to pass its receiver argument through
 * exactly as this-injection typed it -- the CALLING class's own receiver
 * type -- even when the callee's OWN declared receiver type was an
 * ancestor's. Concretely: `Circle::describeTwice` (tests/sample14.cpp)
 * calls the inherited virtual `area()`, and separately the inherited
 * non-virtual `describe()`; in BOTH cases a `Circle *this` was being
 * passed where the callee expects `Shape *`, no cast, which is at
 * minimum a warning in standard C and given how strict Vircon32 has
 * shown itself to be elsewhere, plausibly a hard error there.
 *
 * FIXED once Matthew confirmed Vircon32 accepts an explicit C-style cast
 * (tested `(Node *) 0` directly against the real compiler) -- that was
 * the missing piece; guessing at a fix without knowing casts were even
 * supported would have repeated the same mistake this project has
 * already avoided twice by testing instead of assuming. lower.c's
 * finalize_call now inserts an explicit `(Type *)` cast on a receiver
 * argument whenever the callee's actual declaring class (found via the
 * new, shared find_declaring_class in sema.h) differs from the caller's
 * own static receiver type -- for BOTH the virtual-dispatch path and the
 * direct non-virtual call path, since tracing showed both are affected
 * the same way. A new AST_CAST node kind (ast.h) carries this through to
 * codegen, which prints it as `((Type *)expr)`. Verified by hand against
 * tests/sample14.cpp's exact call sites, including confirming the fix
 * does NOT over-apply: `Shape::describe`'s own call to `area()` (where
 * caller and callee's declaring class are the same, no mismatch exists)
 * correctly gets no cast at all. STILL NEEDS a real compile to confirm
 * this reasoning holds against the actual compiler, the same as every
 * other quirk in this file -- reasoning correctly through the mechanism
 * is not the same thing as a confirmed working build.
 *
 * A SEPARATE, ADJACENT GAP NOTICED WHILE TRACING THE ABOVE: this project
 * has no special handling anywhere for a user-defined `main` -- it gets
 * mangled like any other free function (`main__void` for
 * tests/sample2.cpp's `int main()`), so the generated C has no actual
 * `main` entry point at all, and nothing enforces Vircon32's `void
 * main()`-with-no-return-value requirement on the user's source `main`
 * either. Not addressed here -- this needs an actual design decision
 * (special-case the name at mangling time? require `void main()` in the
 * C++ source and reject anything else? synthesize a wrapper?), not a
 * quick fix bundled into this round.
 */

/* Emits generated Vircon32 C source for the whole program to `out`
 * (already open for writing). Currently emits: a forward declaration for
 * every class, top-level typedefs, each class's vtable struct type (if
 * it has any virtual methods), each class's own struct definition, a
 * prototype for every method/function that has a body, then every
 * method/function body itself -- see the scope notes above for what's
 * still a known gap rather than done. */
void codegen_run(const AstNode *program, FILE *out);

#endif /* CODEGEN_H */
