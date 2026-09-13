#ifndef LOWER_H
#define LOWER_H

#include "ast.h"

/*
 * Lowering runs in bite-sized phases, each a distinct transformation
 * toward what the generated C needs to look like. So far:
 *
 *   Phase 1: computes a flattened, C-struct-ready field layout for every
 *   class -- the exact, ORDERED sequence of fields a generated
 *   `struct ClassName { ... };` needs.
 *
 *   "Flattened" is the important word: a derived class's fields are NOT
 *   "the base's struct, nested, plus my own fields" -- they're the
 *   base's fields copied in directly as a literal PREFIX of the derived
 *   class's own field list, in the same order the base itself would
 *   use. That's what makes single-inheritance polymorphism work the
 *   same way in C as it does in real C++: a `Derived*` can be safely
 *   used wherever a `Base*` is expected, because the first N fields of a
 *   Derived object, byte for byte, ARE a Base object.
 *
 *   The vtable pointer (when a class has one) is placed FIRST in
 *   whichever class's field list first introduces virtual-ness in its
 *   hierarchy -- every class further derived from it inherits that SAME
 *   field (same name, same position) rather than growing a second one.
 *   A class with no virtual methods anywhere in its ancestry gets no
 *   vtable pointer field at all.
 *
 *   Phase 2: this-injection. Turns a method's implicit receiver into an
 *   explicit first parameter (`ClassName *this`), and rewrites every
 *   reference to it -- `this` itself, and any bare identifier that
 *   implicitly meant `this->something`, INCLUDING an unqualified call to
 *   another method -- into an explicit form built on that parameter.
 *   After this phase, a method body has no implicit member access left
 *   in it at all. Reuses the exact same "is this bare name a local, or
 *   does it mean a member" logic access-control enforcement already
 *   established (find_member_in_hierarchy/find_local/LocalVarType,
 *   exposed from sema.h for this purpose) rather than risking a second,
 *   subtly different copy of that logic drifting out of sync with the
 *   original over time.
 *
 *   Phase 3: vtable dispatch codegen (call finalization). Rewrites every
 *   call's callee into its final form: a virtual method call becomes
 *   `obj->vtable->FIELD(obj, args...)`; a non-virtual method call
 *   becomes a direct call to the mangled function name (with `obj`
 *   prepended as the first argument, matching this-injection's
 *   convention); a free-function call becomes a direct call to its own
 *   mangled name. Driven by sema.c's CallResolution (already overload-
 *   aware) rather than re-resolving names independently -- a call sema
 *   couldn't resolve is left completely untouched, same best-effort
 *   philosophy as everywhere else in this project. The vtable FIELD NAME
 *   used at a call site always comes from a slot's `canonical_method`
 *   (whichever class ORIGINALLY declared it virtual), never from
 *   whichever override the call actually resolves to -- that's what
 *   makes the field name STABLE across an entire hierarchy, which is the
 *   whole point of a vtable: the same field, looked up the same way,
 *   regardless of the object's actual runtime type.
 *
 *   Phase 4: operator-overload-to-function-call rewriting. Resolves
 *   natural operator syntax (`a + b`, `a == b`, `v[i]`) against a class's
 *   declared `operatorX` overloads (member first, respecting name-
 *   hiding; a free function as a fallback) -- something sema_run() never
 *   did (it only ever resolved explicit call syntax). On a match, builds
 *   the equivalent AST_CALL and hands it straight to phase 3's own
 *   finalize_call, reusing all of its dispatch logic. On no match, the
 *   node is left as a plain built-in operation. KNOWN GAP: unlike a
 *   regular call, an operator-overload mismatch here isn't diagnosed
 *   (no "no matching operator" error) and isn't access-control checked
 *   -- this resolution happens at lowering time, after sema_run() has
 *   already finished doing both of those things for ordinary calls.
 *
 *   Phase 5: reference-to-pointer rewriting. Every AST_REFERENCE_TYPE in
 *   a parameter or local variable's declared type becomes
 *   AST_POINTER_TYPE, and every explicit `.` access through a bare
 *   identifier naming one of those gets rewritten to `->` (a plain
 *   by-value local's `.` access is untouched). Scope limitation: only
 *   tracks reference-ness for a bare identifier, not through a longer
 *   member-access chain.
 *
 *   Phase 6: new/delete-to-runtime-call rewriting -- DELIBERATELY A
 *   PLACEHOLDER. `new T` becomes a call to a per-type stub allocator
 *   (`v32_new_T`); `delete expr` becomes a call to a single generic stub
 *   deallocator (`v32_delete`). Neither invokes a constructor or
 *   computes a real size -- this project has no `sizeof` AST
 *   representation, and `new`'s grammar (`unary_expr: NEW type_spec` in
 *   parser.y) has never supported constructor arguments at all
 *   (`new Foo(1, 2)` isn't parseable). This phase exists so the AST has
 *   SOME concrete, C-shaped call here rather than an unlowerable
 *   AST_NEW/AST_DELETE surviving into codegen; the real runtime library
 *   and the grammar fix are both tracked as future work.
 *
 * NOT done yet: actually emitting any of the above as C text. Every
 * phase so far only produces a data structure or a mutated AST, never
 * generated syntax -- that's the Vircon32 C code generator's job, still
 * ahead.
 *
 * PRECONDITION: sema_run() must have already completed successfully
 * (zero errors) before lower_run() is called -- these phases read each
 * class's ClassLayout (sema_info) and trust it's complete and correct;
 * they do no validation of their own; running them against a program
 * sema already flagged errors in has undefined results. Also: sema_run()
 * must never be called AGAIN on an AST that's already been through
 * this-injection -- phase 2 mutates each method's parameter list in
 * place, and sema passes like attach_out_of_line's signature matching
 * would see the injected "this" parameter and misbehave. Not a concern
 * for main.c's current single-pass pipeline; worth remembering if this
 * project ever grows an incremental/re-analysis mode.
 *
 * ALSO: sema_cleanup() (see sema.h) must NOT be called until AFTER
 * lower_run() has finished. Phase 3 (call finalization) depends on
 * sema.c's class registry via resolve_expr_class -- freeing it any
 * earlier silently breaks virtual-call lowering (a real bug this project
 * shipped once already; see the postmortem in docs/DESIGN_NOTES.md).
 */

typedef enum {
    FIELD_VTABLE_PTR,   /* the vtable pointer -- always index 0 in
                          * whichever class's StructLayout first
                          * introduces it; every field after it in that
                          * same list (and every field in a derived
                          * class's list, since it's copied forward) is a
                          * data member. */
    FIELD_DATA_MEMBER
} StructFieldKind;

typedef struct StructField {
    StructFieldKind kind;
    const char *name;          /* "vtable" for FIELD_VTABLE_PTR; the
                                 * member's own name for FIELD_DATA_MEMBER. */
    AstNode *type;             /* declared type, for FIELD_DATA_MEMBER only
                                 * (NULL for FIELD_VTABLE_PTR -- a vtable
                                 * pointer's C type is a synthesized
                                 * function-pointer-table pointer, not
                                 * something with source-level AST type
                                 * syntax to point at). */
    AstNode *source_member;    /* the AST_VAR_DECL this field came from, or
                                 * NULL for FIELD_VTABLE_PTR. */
    AstNode *declaring_class;  /* which class in the hierarchy actually
                                 * declared this field -- itself, or an
                                 * ancestor it was flattened in from. */
} StructField;

typedef struct StructLayout {
    StructField *fields;
    int count;
    int capacity;
    int vtable_ptr_index;      /* index into `fields` of the vtable
                                 * pointer, or -1 if this class (and its
                                 * whole ancestry) has none. */
} StructLayout;

/* Runs all lowering phases implemented so far, in order, over every
 * class in the program (recursing into namespaces): phase 1 (struct
 * field layout, attached to each class's `lower_info`), phase 2
 * (this-injection), phase 3 (call finalization/vtable dispatch, with
 * phase 4's operator-overload rewriting living inside that same walk),
 * phase 5 (reference-to-pointer), then phase 6 (new/delete placeholder
 * calls) -- every phase from 2 onward mutates method bodies/parameter
 * lists in place. Always succeeds (0) -- there's no new validation
 * happening here, just transformation of already-sema-validated data;
 * a nonzero return is
 * reserved for a later phase that might have something to report. */
int lower_run(AstNode *program);

/* Prints each class's flattened field layout (phase 1's output -- name,
 * kind, declared type rendered in ordinary C++-like syntax rather than
 * sema.c's mangling-safe form, and, for an inherited field, which
 * ancestor actually declared it), followed by every method's now-
 * fully-lowered body (phases 2 through 6's combined output, reusing
 * ast_dump() -- these are just ordinary AstNode trees, now mutated, so
 * nothing about displaying them needs to be lowering-specific). Same
 * role sema_dump() plays for semantic analysis. */
void lower_dump(const AstNode *program);

#endif /* LOWER_H */
