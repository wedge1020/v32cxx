#ifndef LOWER_H
#define LOWER_H

#include "ast.h"

/*
 * Phase 1 of lowering: computes a flattened, C-struct-ready field layout
 * for every class -- the exact, ORDERED sequence of fields a generated
 * `struct ClassName { ... };` needs.
 *
 * "Flattened" is the important word: a derived class's fields are NOT
 * "the base's struct, nested, plus my own fields" -- they're the base's
 * fields copied in directly as a literal PREFIX of the derived class's
 * own field list, in the same order the base itself would use. That's
 * what makes single-inheritance polymorphism work the same way in C as
 * it does in real C++: a `Derived*` can be safely used wherever a
 * `Base*` is expected, because the first N fields of a Derived object,
 * byte for byte, ARE a Base object.
 *
 * The vtable pointer (when a class has one) is placed FIRST in whichever
 * class's field list first introduces virtual-ness in its hierarchy --
 * every class further derived from it inherits that SAME field (same
 * name, same position) rather than growing a second one. A class with no
 * virtual methods anywhere in its ancestry gets no vtable pointer field
 * at all.
 *
 * This is deliberately scoped to just that computation. NOT done yet
 * (later lowering phases, in order): actually emitting a `struct`
 * definition as C text, `this`-injection on methods (turning an implicit
 * receiver into an explicit first parameter), vtable dispatch codegen
 * (turning a virtual call into an indirect call through the field this
 * phase locates), operator-overload-to-function-call rewriting,
 * reference-to-pointer rewriting, and new/delete-to-runtime-call
 * rewriting.
 *
 * PRECONDITION: sema_run() must have already completed successfully
 * (zero errors) before lower_run() is called -- this phase reads each
 * class's ClassLayout (sema_info) and trusts it's complete and correct;
 * it does no validation of its own; running it against a program sema
 * already flagged errors in has undefined results.
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

/* Runs phase 1 over every class in the program (recursing into
 * namespaces), attaching a StructLayout to each class's `lower_info`.
 * Always succeeds (0) -- there's no new validation happening here, just
 * recomputation from already-sema-validated data; a nonzero return is
 * reserved for later lowering phases that might have something to
 * report. */
int lower_run(AstNode *program);

/* Prints each class's flattened field layout -- name, kind, declared
 * type (rendered in ordinary C++-like syntax, not sema.c's mangling-safe
 * form), and, for an inherited field, which ancestor actually declared
 * it. Same role sema_dump() plays for semantic analysis. */
void lower_dump(const AstNode *program);

#endif /* LOWER_H */
