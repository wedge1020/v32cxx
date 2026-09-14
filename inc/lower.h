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
 *   Phase 6: new/delete-to-runtime-call rewriting. `new T`/`new T(args)`
 *   becomes a call to a per-constructor-overload allocator (named after
 *   the resolved constructor's own mangled name, or the bare type name
 *   if the class has no constructor at all -- see lower.c's own doc
 *   comment on new_delete_rewrite_expr for exactly why per-overload
 *   naming matters); `delete expr` becomes a call to a per-class
 *   deallocator (`v32_delete_ClassName`, named after the operand's own
 *   static class -- falls back to a single, fully generic `v32_delete`
 *   only when that class can't be determined at all). All of these
 *   actually get DEFINED now, using Vircon32's real `malloc()`/`free()`
 *   (`misc.h`, confirmed against the real Vircon32 C standard library,
 *   not invented) -- see codegen.c's emit_new_delete_runtime/
 *   emit_delete_runtime. `v32_new_*` allocates AND constructs (calls the
 *   resolved constructor on the freshly-allocated memory); the
 *   per-class `v32_delete_*` calls the class's own destructor (if one
 *   exists with a body) before freeing. This closes the gap
 *   `tests/sprite.cpp` (now `tests/sample23.cpp`) found directly:
 *   `v32_new_Player` used to be an undefined stub, "identifier ... has
 *   not been declared"; it's now an actual function that allocates via
 *   `malloc(sizeof(Player))` and calls `Player::Player()` on the
 *   result. `tests/sample25.cpp` is the equivalent confirmation for the
 *   destructor side.
 *
 *   STILL MISSING, deliberately: `sizeof` isn't a real AST concept in
 *   this project -- codegen.c emits the literal text `sizeof(TypeName)`
 *   directly rather than computing anything itself, which works fine
 *   for this specific purpose (the C compiler evaluates it, not this
 *   one) but means there's still no general `sizeof` expression support
 *   for C++ source that might want to use one.
 *
 *   VIRTUAL DESTRUCTOR DISPATCH: FIXED, as of a later round still --
 *   `delete basePtr;` through an ancestor-typed pointer now correctly
 *   calls the DERIVED destructor, not the ancestor's own, whenever the
 *   destructor is virtual (declared so, or overriding one). Turned out
 *   NOT to need any new vtable machinery at all: a virtual destructor
 *   already participated correctly in sema.c's existing vtable-slot
 *   assignment (build_vtable doesn't special-case destructors, just
 *   checks the generic "is this virtual" flag any method has, and
 *   vtable_slot_key already normalizes every destructor's own name to
 *   the literal string "~" regardless of class, so a derived override
 *   already matched its base's inherited slot the same way an ordinary
 *   virtual method does). The only actual gap was in codegen.c's
 *   emit_delete_runtime -- the per-static-type deallocator
 *   (v32_delete_ClassName) always called a statically-named function
 *   regardless of whether the destructor was virtual. Fixed there
 *   specifically, not in lower.c's own AST_DELETE naming logic (which
 *   was never the problem -- naming the deallocator after the operand's
 *   STATIC class is still exactly correct; what that per-class function
 *   does INTERNALLY is what needed to change): a virtual destructor now
 *   makes v32_delete_ClassName dispatch through `ptr->vtable->...`
 *   instead, the same shape finalize_call already builds for an
 *   ordinary virtual method call, with the same receiver-cast reasoning
 *   (the vtable access itself never needs a cast; the argument passed
 *   to the slot does, whenever the class isn't its own canonical
 *   declarer). `tests/sample32.cpp` (Shape/Square, Shape's destructor
 *   virtual, `delete` through a `Shape *` that actually points at a
 *   `Square`) is the test -- traced by hand and confirmed correct
 *   (`ptr->vtable->Shape__dtor__void(ptr)` correctly resolves to
 *   `Square__dtor__void` at runtime, since a Square's own vtable
 *   instance is what `ptr->vtable` actually points at), but NOT yet
 *   build-confirmed at all as of this writing -- this sandbox didn't
 *   have a working `lexer.c` available at the time (a stale one, from
 *   several rounds back, missing even the preprocessor-pass-through
 *   support added since), so nothing from this fix has been run through
 *   an actual compile yet, on either side.
 *
 *   ALSO HANDLES, as of a later round: `new T[N]` (array-new) and
 *   `delete[]`. Array-new is a COMPLETELY separate shape from
 *   single-object `new`, named "v32_new_arr_ClassName" (distinct from
 *   the per-constructor-overload family above, and never ambiguous the
 *   same way -- there's exactly one shape of array-new per class,
 *   regardless of what constructors it has) and deliberately
 *   allocation-only: `malloc(N * sizeof(ClassName))`, cast, return,
 *   nothing more -- no per-element construction happens at all, since
 *   there's no per-element analogue of phase 7's stack-array support or
 *   any loop-emission machinery in this project. `delete[]` currently
 *   lowers IDENTICALLY to plain `delete` (the distinction is recorded
 *   on the AST, AST_DELETE's own `ival`, but nothing downstream acts on
 *   it yet) -- for the same reason: no per-element destructor
 *   invocation exists for either new[] or delete[] yet either.
 *
 *   Phase 7: constructor invocation for stack-allocated locals. A plain
 *   `ClassName var;` declaration with no explicit initializer now calls
 *   a matching zero-argument constructor, if one exists and has a body,
 *   immediately after the declaration -- closing the gap
 *   tests/sample22.cpp (a real, hand-written program, not an artificial
 *   unit test) found: `Player sprite;` used to compile cleanly but never
 *   call `Player::Player()` at all, leaving `sprite.x`/`sprite.y` as
 *   uninitialized stack garbage. Deliberately narrow scope for this
 *   first round: only a zero-argument constructor is matched (this
 *   syntax form has no way to pass arguments); a prototype-only
 *   constructor with no body is skipped rather than called (calling one
 *   would repeat the exact `v32_new_Player`-shaped mistake); a VarDecl
 *   inside a for-loop's own init clause isn't handled (`for (Player p;
 *   ...)` -- inserting the call would need to land inside the loop body
 *   instead of right after the declaration, more involved for a pattern
 *   nothing currently exercises). A class WITH virtual methods gets its
 *   constructor called by this phase same as any other -- see phase 8,
 *   below, for how its vtable pointer actually gets populated now too.
 *
 *   Phase 8: vtable pointer initialization in constructors. NOTE: this
 *   paragraph was originally missing from this file's own documentation
 *   entirely -- added retroactively while writing phase 9's own entry
 *   below and noticing the gap, not at the time phase 8 itself was
 *   built. For every class WITH a vtable, prepends
 *   `this->vtable = &ClassName_vtable_instance;` to the very start of
 *   each of its own constructors that has a body -- before anything the
 *   constructor's own body does, matching real C++'s own vtable-
 *   initialization timing. `ClassName_vtable_instance` is codegen.c's
 *   emit_vtable_instance -- a populated, static instance of the class's
 *   own vtable struct, with each slot holding whichever implementation
 *   actually applies at that level (an override, if one exists),
 *   correctly cast when that implementation's own declaring class
 *   differs from the slot's canonically-declared one. Together with
 *   phase 7, this makes construction of a polymorphic object -- and
 *   virtual dispatch on the result -- fully correct: confirmed against
 *   the real compiler end to end (`tests/sample24.cpp`, a `Shape`/
 *   `Square` pair with a virtual `area()`, an override, and a real
 *   constructor on each -- deliberately not reusing `tests/sample7.cpp`/
 *   `sample12.cpp`/`sample14.cpp`, none of which declare a constructor
 *   at all, so none of them would have exercised this at all). A class
 *   with virtual methods but NO constructor at all (or only a bodyless
 *   one) still has no way to get its vtable pointer populated -- there's
 *   nowhere for this phase to inject into. Real C++ would synthesize an
 *   implicit default constructor for such a class; this project doesn't.
 *
 *   Phase 9: destructor invocation at scope exit. The mirror of phase 7,
 *   for teardown instead of construction. A stack-allocated local of
 *   class type, whose class has a destructor with a body, now gets that
 *   destructor called wherever it goes out of scope -- both falling off
 *   the end of its enclosing block, and an early `return`, in reverse
 *   declaration order. This is the COMPLETE picture for THIS PROJECT AS
 *   ITS GRAMMAR CURRENTLY STANDS, not a scoped-down first slice of a
 *   bigger problem: real C++ RAII also has to handle
 *   `break`/`continue`/exceptions unwinding a scope early, but this
 *   project's grammar has neither `break` nor `continue` at all right
 *   now (confirmed directly -- no token, no AST kind, nothing in
 *   lexer.l or parser.y), and exceptions are out of scope for this
 *   project entirely. Fall-through and `return` are the only two ways
 *   control can leave a block in the language this project currently
 *   accepts, so handling both really is the general solution as things
 *   stand today.
 *
 *   REVISITED, AS PROMISED: `break`/`continue` now exist in this
 *   project's grammar (parser.y/lexer.l), and this phase now treats
 *   either as a THIRD early-exit path, alongside fall-through and
 *   `return`. Unlike `return`, which destroys everything from the
 *   current scope all the way to the function's own top, a
 *   `break`/`continue` only destroys what's live from the current scope
 *   up to (but not including) the boundary of the loop actually being
 *   exited -- anything declared in a scope enclosing that loop stays
 *   alive, exactly as it would after the loop ends normally too. Tracked
 *   via a new `loop_boundary` parameter threaded through
 *   destruct_scope_stmt/destruct_scope_block (lower.c) -- AST_WHILE/
 *   AST_FOR set a fresh boundary (the scope in effect right before
 *   entering their own body) when recursing into their body, and
 *   AST_IF passes whatever boundary it was already given straight
 *   through unchanged, since an `if` doesn't introduce a loop of its
 *   own. sema.c separately rejects a `break`/`continue` appearing
 *   outside any loop at all (real C++/C both do too), via a simple
 *   loop-depth counter incremented/decremented around a loop body's own
 *   walk -- not threaded through check_node's parameter list, since
 *   that walk is single-threaded and strictly depth-first, so a
 *   file-local global serves the same purpose far more simply.
 *
 *   An early `return expr;` needs a small rewrite -- `expr` must be
 *   evaluated before any destructor runs, so it becomes a nested block
 *   holding the already-computed result in a temporary, the destructor
 *   calls, then a bare `return` of the temporary. A KNOWN, minor
 *   inefficiency, not a correctness issue: a block whose own last
 *   statement is always a `return`/`break`/`continue` still gets a
 *   fall-through destructor sequence appended after it, which is then
 *   simply unreachable -- see lower.c's own doc comment on this phase
 *   for why detecting that would need real reachability analysis, not
 *   attempted here.
 *
 *   CONFIRMED against the real Vircon32 compiler for the fall-through/
 *   return paths (Matthew's report, `tests/sample27.cpp`). The
 *   break/continue extension itself is NOT yet confirmed the same way
 *   as of this writing -- `tests/sample30.cpp` (a loop constructing a
 *   destructible local every iteration, exercising `break`, `continue`,
 *   AND ordinary fall-through as three separate exits from the same
 *   loop body) is the test written for it, and the C-side logic
 *   compiles and links cleanly with zero regressions across the full
 *   existing suite -- but the grammar itself (parser.y/lexer.l) still
 *   needs a fresh bison/flex build from Matthew before `break`/
 *   `continue` can even be parsed at all, let alone confirmed correct
 *   end to end. `tests/sample31.cpp` (a deliberately invalid
 *   `continue;` outside any loop) is the matching test for sema.c's new
 *   validity check, same status.
 *
 * NOT done yet: base-class constructor delegation (C++ member-
 * initializer lists, `Derived::Derived() : Base(args) {}`) -- a
 * derived class's constructor still has to set inherited fields
 * directly, the way tests/sample24.cpp's Square already does.
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
 * (this-injection), phase 8 (vtable pointer init in constructors --
 * runs here, right after phase 2, so every later phase sees it as
 * simply the first statement already present), phase 3 (call
 * finalization/vtable dispatch, with phase 4's operator-overload
 * rewriting living inside that same walk), phase 5
 * (reference-to-pointer), phase 6 (new/delete runtime calls), phase 7
 * (constructor invocation for stack-allocated locals), then phase 9
 * (destructor invocation at scope exit) -- every phase from 2 onward
 * mutates method bodies/parameter lists in place. Always succeeds (0)
 * -- there's no new validation happening here, just transformation of
 * already-sema-validated data; a nonzero return is
 * reserved for a later phase that might have something to report. */
int lower_run(AstNode *program);

/* Prints each class's flattened field layout (phase 1's output -- name,
 * kind, declared type rendered in ordinary C++-like syntax rather than
 * sema.c's mangling-safe form, and, for an inherited field, which
 * ancestor actually declared it), followed by every method's now-
 * fully-lowered body (phases 2 through 9's combined output, reusing
 * ast_dump() -- these are just ordinary AstNode trees, now mutated, so
 * nothing about displaying them needs to be lowering-specific). Same
 * role sema_dump() plays for semantic analysis. */
void lower_dump(const AstNode *program);

#endif /* LOWER_H */
