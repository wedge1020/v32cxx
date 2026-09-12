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
 * NOT done yet (later lowering phases, in order): actually emitting a
 * `struct` definition or a method's new signature/body as C text (both
 * phases above only produce data structures / a mutated AST, not
 * generated syntax); vtable dispatch codegen (turning a virtual call
 * into an indirect call through the field phase 1 locates);
 * operator-overload-to-function-call rewriting; reference-to-pointer
 * rewriting; and new/delete-to-runtime-call rewriting.
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
 * field layout, attached to each class's `lower_info`) then phase 2
 * (this-injection, which mutates method bodies/parameter lists in
 * place). Always succeeds (0) -- there's no new validation happening
 * here, just transformation of already-sema-validated data; a nonzero
 * return is reserved for a later phase that might have something to
 * report. */
int lower_run(AstNode *program);

/* Prints each class's flattened field layout (phase 1's output -- name,
 * kind, declared type rendered in ordinary C++-like syntax rather than
 * sema.c's mangling-safe form, and, for an inherited field, which
 * ancestor actually declared it), followed by every method's now-
 * this-injected body (phase 2's output, reusing ast_dump() -- these are
 * just ordinary AstNode trees, now mutated, so nothing about displaying
 * them needs to be lowering-specific). Same role sema_dump() plays for
 * semantic analysis. */
void lower_dump(const AstNode *program);

#endif /* LOWER_H */
