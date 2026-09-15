#ifndef SEMA_H
#define SEMA_H

#include "ast.h"

/*
 * First slice of semantic analysis. Deliberately narrow in scope -- see
 * the per-pass comments in sema.c and the README's "what's not here yet"
 * list for what this does NOT do (inherited-DATA-member layout merging,
 * multiple inheritance -- not planned at all, see the project README).
 *
 * What it DOES do, run in this order by sema_run():
 *   1. Walk the whole program (recursing into namespaces) and register
 *      every class, typedef, AND free function by name into flat
 *      registries (free functions grouped, not deduplicated -- a name
 *      can have several entries, which is exactly what an overload set
 *      is).
 *   2. Attach out-of-line definitions (FUNC_DEF nodes produced by
 *      out_of_line_def in parser.y, identifiable by a non-NULL `b`
 *      qualifier chain) onto the matching in-class prototype -- matched
 *      by name AND parameter-type signature (see types_equal/
 *      param_lists_match in sema.c), so this correctly picks the right
 *      overload rather than just the first same-named prototype -- turning
 *      that prototype into the authoritative FUNC_DEF. The top-level
 *      duplicate is left in place but flagged (FuncSemaInfo.is_out_of_line)
 *      so a later codegen pass knows to skip re-emitting it.
 *   3. Compute a ClassLayout for every class: its data members and methods
 *      split out from AccessSpec markers (each member's OWN access level
 *      -- public/private/protected -- is stamped onto it as this walk
 *      happens, defaulting to private per `class`'s C++ default when no
 *      marker precedes the first member), its resolved base class (by AST
 *      pointer, not just name, plus the inheritance access-specifier
 *      carried on AST_CLASS_DECL.access), and its vtable (see
 *      build_vtable() in sema.c) -- assigning a slot to every virtual
 *      method, correctly reusing the base's slot for an override (even
 *      one that doesn't repeat the `virtual` keyword, matching real C++)
 *      rather than creating a second, unrelated slot.
 *   4. Assign every method (and every free function) a mangled name that
 *      folds in a parameter-type signature (see mangle() in sema.c), so
 *      overloads -- including out-of-line-defined ones -- get distinct
 *      names instead of colliding.
 *   5. Enforce access control: walk every method/function BODY and check
 *      every actual member reference (explicit `.`/`->`, or an implicit
 *      `this->` via a bare name that resolves to an INHERITED member)
 *      against the accessing class's relationship to whichever class
 *      declared that member -- see the long comment above
 *      check_member_access() in sema.c for the exact rule and its
 *      deliberately bounded scope (best-effort: it only checks what it
 *      can confidently resolve the type of; anything else is silently
 *      skipped, never falsely flagged either way).
 *   6. Resolve call-site overloads: for every AST_CALL, figure out which
 *      specific candidate (by name, then by argument count, then by
 *      argument type when more than one candidate shares that count) the
 *      call actually refers to, attaching the answer as a CallResolution
 *      on the call's sema_info (see below). Runs as part of the SAME
 *      body walk as #5 (inside check_node's AST_CALL case in sema.c),
 *      not a separate pass over the program -- both need the same
 *      "calling context" walk through every method/function body. Same
 *      best-effort philosophy as #5: an argument whose type can't be
 *      confidently determined means the call is left unresolved rather
 *      than guessed at, UNLESS there's only one candidate by that name at
 *      all, in which case there's no real ambiguity to resolve and arity
 *      alone is enough. NOT implemented: any notion of implicit
 *      conversions -- argument types must match a candidate's parameter
 *      types exactly (typedef-transparent, nothing more permissive).
 *
 * NOTE ON #2, #3's vtable matching, #4, and #6: the type comparison
 * behind all of these (types_equal/param_lists_match in sema.c) is
 * typedef-transparent as of this pass -- `void f(int)` and
 * `void f(MyIntTypedef)` are correctly treated as the same signature,
 * resolving through typedef-of-typedef chains too. See
 * resolve_typedef_chain()'s doc comment in sema.c for exactly what it
 * does and does not chase (it's name-based typedef resolution, not full
 * semantic type equivalence -- e.g. it has no notion of `const`, since
 * this project's type grammar doesn't parse cv-qualifiers at all yet).
 *
 * NOTE ON #3's vtable: this assigns slot NUMBERS and resolves which
 * AstNode implements each slot for each class -- it does not generate
 * an actual C vtable struct or the function-pointer-table initializer
 * that a lowering/codegen pass would eventually need to emit. That's
 * deliberately a separate, later step; see sema_dump()'s vtable output
 * for what's available to build on.
 */

typedef struct VtableEntry {
    AstNode *method;    /* the AST_FUNC_DECL/AST_FUNC_DEF providing THIS
                         * class's implementation for the slot -- either
                         * this class's own override, or (if not
                         * overridden here) the same node inherited
                         * unchanged from the base's vtable. */
    AstNode *canonical_method; /* the ORIGINAL declaration that first
                         * introduced this slot, in whichever class's
                         * hierarchy first declared it as virtual --
                         * stays FIXED across every derived class's copy
                         * of this slot, even when `method` above is
                         * overridden further down. lower.c's vtable-
                         * dispatch phase needs this: a call site like
                         * `obj->vtable->FIELD(obj)` must use the SAME
                         * field name regardless of the object's actual
                         * runtime type, since that's what the generated
                         * vtable STRUCT TYPE names the field -- only the
                         * function pointer VALUE stored there varies per
                         * class, which is a separate, later concern
                         * (initializing each class's vtable instance),
                         * not something this field or lower.c's current
                         * phase does. */
    int slot_index;
} VtableEntry;

typedef struct Vtable {
    VtableEntry *entries;
    int count;
    int capacity;
} Vtable;

typedef struct ClassLayout {
    AstList data_members;      /* AST_VAR_DECL nodes, in declaration order */
    AstList methods;           /* AST_FUNC_DECL/AST_FUNC_DEF nodes, in declaration order */
    AstNode *base_class_decl;  /* resolved AST_CLASS_DECL of the base class, or NULL.
                                 * NOTE: inherited DATA members are NOT merged into
                                 * data_members/methods above -- a consumer that
                                 * needs "all members including inherited ones"
                                 * has to walk base_class_decl's own ClassLayout
                                 * (via its sema_info) explicitly. Not merged
                                 * automatically to avoid silently duplicating
                                 * members if layout computation ever runs more
                                 * than once over the same AST. (Virtual METHODS
                                 * are handled differently -- see `vtable` below,
                                 * which does carry inherited slots forward, since
                                 * that's what a vtable needs to do.) */
    Vtable *vtable;             /* NULL if this class has no virtual methods,
                                 * own or inherited. Otherwise, one slot per
                                 * distinct virtual method in the hierarchy
                                 * (by name+signature, with all destructors
                                 * across the hierarchy sharing one slot --
                                 * see vtable_slot_key() in sema.c), in the
                                 * order first introduced by the base-most
                                 * class that declared each one. A derived
                                 * class's vtable is NOT just its own new/
                                 * overridden methods -- it's the base's
                                 * vtable, copied, with overridden slots
                                 * repointed at this class's own
                                 * implementation and new virtual methods
                                 * appended after. */
} ClassLayout;

/* Attached to a node's sema_info once overload resolution determines
 * which specific function/method it refers to -- an AST_CALL (via
 * resolve_call), an AST_NEW (via resolve_new_expr, resolving which
 * constructor overload), or a BinOp/Assign/Unop/Subscript used as
 * natural operator syntax (via resolve_operator_use). See the doc
 * comment on AST_CALL in ast.h for exactly what it means for a call to
 * NOT have this attached. */
typedef struct CallResolution {
    AstNode *resolved_target;  /* the specific FUNC_DECL/FUNC_DEF this call
                                * resolves to. */
    int is_member;             /* Only meaningful for an operator-overload
                                * resolution (AST_CALL/AST_NEW always know
                                * this structurally from their own shape --
                                * an AST_CALL's callee's OWN kind, AST_MEMBER
                                * vs AST_IDENT, already says it; an AST_NEW
                                * is always a member/constructor). 1 if
                                * `resolved_target` is a class member (so
                                * lowering needs to prepend the receiver as
                                * an explicit argument when rewriting this
                                * into a real call), 0 if it's a free
                                * function (no implicit receiver at all). */
} CallResolution;

typedef struct FuncSemaInfo {
    char *mangled_name;   /* e.g. "Player__update__void" for a no-arg
                            * method, "Counter__Counter__int" for a
                            * one-int-param constructor, or "Counter__dtor__void"
                            * for its destructor (the mangled name can't
                            * contain the literal "~" from the AST's
                            * "~Counter" spelling, since that's not a legal
                            * C identifier character -- see mangle() in
                            * sema.c). Free functions omit the class
                            * component: "clamp__int_int_int". Two
                            * overloads (different parameter types) of the
                            * same name now get distinct mangled names --
                            * see the syntactic-vs-semantic type comparison
                            * caveat on the file-level comment above. */
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

/* Checks whether the program defines an actual top-level `main` (an
 * AST_FUNC_DEF, not merely a prototype). Call ONLY after sema_run() has
 * completed with zero errors -- this doesn't validate anything about
 * the AST itself, it's purely a query, and main.c uses it for its own
 * CLI-level "require a complete program by default, `-c` disables it"
 * behavior. See sema.c's own doc comment on this function for why it's
 * deliberately NOT something sema_run() itself checks or cares about. */
int sema_program_has_main(const AstNode *program);

/* Generalizes sema_program_has_main to an arbitrary function name --
 * added for main.c's own -b (BIOS) validation, which needs the same
 * "does a function with this name exist anywhere, including inside a
 * namespace" check for `error_handler`. Same call-only-after-sema_run
 * rule applies. */
int sema_program_has_function(const AstNode *program, const char *name);

/* Frees the class/typedef/free-function registries sema_run() builds.
 * Call this ONLY after every pass that might need them has finished --
 * sema_run() itself, AND lower_run() (lower.c's vtable-dispatch phase
 * depends on find_class() via resolve_expr_class, below). Not called
 * automatically at the end of sema_run() specifically because lowering
 * needs these registries to still be alive after sema_run() returns --
 * see the comment in sema_run()'s own implementation for the bug this
 * fixed (virtual method calls silently failing to lower, no crash, no
 * error, just quietly wrong) when that dependency wasn't yet explicit. */
void sema_cleanup(void);

/* A simple, flat, NOT-properly-block-scoped map of local variable/
 * parameter name -> declared type, built while walking a function/method
 * body. See the long comment on this same struct (moved here from
 * sema.c) for the block-scoping caveat: this is exposed (not `static` in
 * sema.c) specifically so lower.c's this-injection phase can reuse the
 * exact same "is this bare name a local, or does it refer to a member"
 * logic access-control enforcement already established, rather than
 * risking a second, subtly different copy of that logic drifting out of
 * sync with this one over time. */
typedef struct LocalVarType {
    const char *name;
    AstNode *type;              /* the declared type, as written */
    int was_reference;          /* 1 if `type` was originally AST_REFERENCE_TYPE
                                  * before lower.c's reference-lowering phase
                                  * relabeled it to AST_POINTER_TYPE -- 0
                                  * (and harmless/unused) for any pass that
                                  * doesn't care, i.e. everything before that
                                  * phase runs. Lets `.` access on a
                                  * reference-turned-pointer local/param be
                                  * correctly rewritten to `->`, while `.`
                                  * access on an ordinary by-value local
                                  * stays `.`. */
    struct LocalVarType *next;
} LocalVarType;

LocalVarType *find_local(LocalVarType *locals, const char *name);

/* Searches `class_decl`'s own members first, then walks up its base
 * chain (single inheritance, so this is a simple chain, not a search
 * tree), looking for a member named `name`. Name-only match (see the
 * TODO in sema.c about overload-aware lookup). Sets *owner_out to the
 * class that ACTUALLY declared the returned member (which may be an
 * ancestor of `class_decl`), needed by callers (access-control's
 * legality check, this-injection's rewriting) to know whether a
 * reference is to this class's own member or an inherited one. */
AstNode *find_member_in_hierarchy(AstNode *class_decl, const char *name, AstNode **owner_out);

/* Infers which class (if any) an expression's static type resolves to --
 * `this`, a local/parameter, a member-access/call chain through either.
 * Returns NULL for anything outside that deliberately bounded scope
 * (arithmetic results, unresolvable names, ...) -- NULL means "unknown",
 * not "not a class", so callers must treat it as "nothing to resolve"
 * rather than an error. Exposed (not `static` in sema.c) for the same
 * reason find_member_in_hierarchy/find_local are: lower.c's vtable-
 * dispatch phase needs to answer the exact same "what class is this
 * call's object expression" question access control already had to
 * answer, and reusing this rather than re-deriving it independently
 * keeps the two from ever quietly disagreeing. */
AstNode *resolve_expr_class(const AstNode *expr, AstNode *current_class, LocalVarType *locals);

/* Infers an expression's declared TYPE (not the class it resolves to --
 * resolve_expr_class, above, is for that) -- e.g. for a bare identifier
 * naming a local/parameter, returns exactly the type node that local was
 * declared with, whether that's a plain class name (a stack-allocated
 * value) or a PointerType/ReferenceType wrapping one. Exposed so
 * lower.c's finalize_call can tell whether an object expression is
 * ALREADY a pointer before deciding whether it needs an explicit
 * address-of to become the pointer a method's receiver parameter
 * requires -- see finalize_call's own doc comment in lower.c for why
 * that distinction matters and what happens without it. */
AstNode *infer_expr_type(const AstNode *expr, AstNode *current_class, LocalVarType *locals);

/* Walks up `class_decl`'s own ancestry (via each class's base_class_decl)
 * to find whichever class's OWN ClassLayout.methods list literally
 * contains `target_method` (a pointer-identity search, not a name match
 * -- a name match could pick the wrong overload/override). Needed
 * anywhere a method/slot's DECLARING class specifically matters, not
 * just which class it's being accessed THROUGH: lower.c's finalize_call
 * uses this to know what pointer type an inherited method's "this"
 * parameter actually needs (an ancestor's, not necessarily the calling
 * object's own runtime type); codegen.c's vtable-struct-type emission
 * uses it for the same reason, to print a vtable field's receiver
 * parameter as the class that first declared it virtual, not whichever
 * class's vtable is currently being emitted. Falls back to `class_decl`
 * itself if the search somehow comes up empty (best-effort, shouldn't
 * happen for a target_method that genuinely came from this hierarchy in
 * the first place). */
const AstNode *find_declaring_class(const AstNode *class_decl, const AstNode *target_method);

/* Resolves a type AST node down to the AST_CLASS_DECL it names (chasing
 * typedefs and unwrapping pointer/reference wrappers), or NULL if it
 * doesn't name a registered class at all. Exposed for the same reuse
 * reason as everything else in this section -- lower.c's new/delete
 * lowering needs to know which class `new T` allocates. */
AstNode *type_to_class(const AstNode *type);

/* Appends every free function/prototype named `name` (ANY arity/
 * signature -- callers filter further themselves) onto `*out`, growing
 * it as needed (same growable-array convention as sema.c's own vtable/
 * candidate-collection code). Exposed so lower.c's operator-overload
 * lowering can look for a free-function `operatorX` the same way
 * sema.c's own call resolution already looks for free functions in
 * general. */
void collect_free_function_candidates(const char *name, AstNode ***out, int *out_count, int *out_cap);

/* Prints a human-readable summary of every class's computed layout,
 * every function's mangled name, and (in a "call resolutions:" section)
 * every call expression that successfully resolved to a specific
 * overload -- useful for eyeballing that sema_run() did what you
 * expected, the same role ast_dump() plays for parsing. The call-
 * resolution section matters more than it might look: a successfully
 * resolved call and a silently-skipped one produce no other visible
 * difference, so this is the only way to positively confirm resolution
 * actually happened rather than just "didn't error". */
void sema_dump(const AstNode *program);

#endif /* SEMA_H */
