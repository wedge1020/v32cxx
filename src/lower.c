#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lower.h"
#include "sema.h"

/* ---- building a StructLayout ------------------------------------------- */

static void struct_layout_append(StructLayout *layout, StructField field) {
    if (layout->count == layout->capacity) {
        layout->capacity = layout->capacity ? layout->capacity * 2 : 4;
        layout->fields = realloc(layout->fields, sizeof(StructField) * (size_t)layout->capacity);
    }
    layout->fields[layout->count++] = field;
}

/* Recursive, base-first -- same shape as sema.c's build_vtable/
 * compute_layout: ensures a class's base has its OWN StructLayout
 * computed before this class copies it forward, regardless of which
 * order the top-level walk (compute_struct_layouts, below) happens to
 * visit classes in. Can't cycle, for the same reason build_vtable's
 * recursion can't: the parser requires a base class to already be a
 * registered TYPE_NAME before it can be named, so a class can never
 * (even transitively) end up inheriting from itself. */
static StructLayout *compute_struct_layout(AstNode *class_decl) {
    if (class_decl->lower_info != NULL) {
        return (StructLayout *)class_decl->lower_info; /* already computed,
            e.g. as another class's base */
    }

    StructLayout *layout = calloc(1, sizeof(StructLayout));
    layout->vtable_ptr_index = -1;

    ClassLayout *sema_layout = (ClassLayout *)class_decl->sema_info;
    /* sema_layout is trusted non-NULL here -- see lower_run()'s
     * precondition: sema_run() must have completed successfully first. */

    if (sema_layout->base_class_decl != NULL) {
        StructLayout *base_layout = compute_struct_layout(sema_layout->base_class_decl);
        /* Copy every field from the base layout forward, IN ORDER, as a
         * literal prefix -- this is what makes a Derived* safely usable
         * as a Base*, the same way it would be in real C++. */
        for (int i = 0; i < base_layout->count; i++) {
            struct_layout_append(layout, base_layout->fields[i]);
        }
        if (base_layout->vtable_ptr_index >= 0) {
            /* Inherited unchanged -- same field, same index, since every
             * one of the base's fields (including its vtable pointer)
             * was just copied forward at the same relative position. */
            layout->vtable_ptr_index = base_layout->vtable_ptr_index;
        }
    }

    /* Does this class need to INTRODUCE a new vtable pointer field?
     * Only if it has a vtable at all (own or inherited virtual methods,
     * per sema.c's build_vtable) AND one wasn't already inherited above. */
    if (sema_layout->vtable != NULL && layout->vtable_ptr_index < 0) {
        StructField vf;
        vf.kind = FIELD_VTABLE_PTR;
        vf.name = "vtable";
        vf.type = NULL;
        vf.source_member = NULL;
        vf.declaring_class = class_decl;
        layout->vtable_ptr_index = layout->count;
        struct_layout_append(layout, vf);
    }

    /* This class's own data members, in declaration order. */
    for (int i = 0; i < sema_layout->data_members.count; i++) {
        AstNode *dm = sema_layout->data_members.items[i];
        StructField f;
        f.kind = FIELD_DATA_MEMBER;
        f.name = dm->str1;
        f.type = dm->type;
        f.source_member = dm;
        f.declaring_class = class_decl;
        struct_layout_append(layout, f);
    }

    class_decl->lower_info = layout;
    return layout;
}

static void compute_struct_layouts(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            compute_struct_layout(n);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            compute_struct_layouts(&n->list);
        }
    }
}

/* ---- phase 2: this-injection --------------------------------------------
 *
 * Turns a method's implicit receiver into an explicit first parameter,
 * and rewrites every reference to it -- `this` itself, and any bare
 * identifier that implicitly meant `this->something` -- into an
 * explicit form built on that parameter. After this phase, a method's
 * body no longer has ANY implicit member access left in it: everything
 * is either a local/parameter reference or an explicit `->` chain.
 *
 * Reuses find_member_in_hierarchy/find_local/LocalVarType from sema.h --
 * this is exactly the same "is this bare name a local, or does it mean a
 * member" question access-control enforcement already had to answer, so
 * this phase answers it the same way rather than risking a second copy
 * of that logic drifting out of sync with the original over time.
 *
 * Scope note, matching the same caveat LocalVarType already carries from
 * access control: local-variable tracking here is flat, not properly
 * block-scoped. A local variable's name correctly SHADOWS a same-named
 * member for as long as it's in scope (checked first, before falling
 * back to member lookup) -- see tests/sample13.cpp for a case that
 * specifically exercises this.
 */

/* Mutates *slot in place -- replaces the AstNode it points to with a
 * rewritten one wherever a `this` or implicit member reference needs to
 * become explicit, and simply recurses (without replacing) everywhere
 * else. Takes AstNode** (a pointer to the SLOT holding the node, i.e.
 * a field like &n->a or an element of a list), not AstNode*, precisely
 * because rewriting sometimes means swapping out which node the parent
 * points to entirely, not just mutating a node already there. */
static void rewrite_expr(AstNode **slot, AstNode *class_decl, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;

    switch (n->kind) {
        case AST_THIS:
            *slot = ast_ident("this", n->line);
            break;
        case AST_IDENT: {
            if (find_local(locals, n->str1) != NULL) {
                break; /* a local/parameter reference, not a member access at all -- leave it alone */
            }
            AstNode *owner = NULL;
            AstNode *member = find_member_in_hierarchy(class_decl, n->str1, &owner);
            if (member != NULL) {
                /* Implicit this->member (a data member OR a method being
                 * called unqualified) -- make it explicit. This also
                 * correctly handles `foo();` meaning `this->foo();`: the
                 * callee identifier gets rewritten to `this->foo` here,
                 * and AST_CALL (below) doesn't need to know or care that
                 * its callee just changed shape. */
                AstNode *mem = ast_new(AST_MEMBER, n->line);
                mem->str1 = strdup("->");
                mem->str2 = strdup(n->str1);
                mem->a = ast_ident("this", n->line);
                *slot = mem;
            }
            break;
        }
        case AST_MEMBER:
            rewrite_expr(&n->a, class_decl, locals);
            break;
        case AST_CALL:
            rewrite_expr(&n->a, class_decl, locals);
            for (int i = 0; i < n->list.count; i++) {
                rewrite_expr(&n->list.items[i], class_decl, locals);
            }
            break;
        case AST_BINOP:
        case AST_ASSIGN:
        case AST_SUBSCRIPT:
            rewrite_expr(&n->a, class_decl, locals);
            rewrite_expr(&n->b, class_decl, locals);
            break;
        case AST_UNOP:
        case AST_DELETE:
            rewrite_expr(&n->a, class_decl, locals);
            break;
        case AST_NEW:
            /* Constructor arguments (if any) can absolutely contain a
             * bare `this` or an implicit member reference -- `new
             * Foo(x, this->y)` -- same as any other expression's
             * arguments. This case didn't exist before this project's
             * grammar supported constructor arguments in a `new`
             * expression at all; it needs to now. */
            for (int i = 0; i < n->list.count; i++) {
                rewrite_expr(&n->list.items[i], class_decl, locals);
            }
            break;
        default:
            /* Literals, AST_QUALIFIED_ID, ... -- nothing to rewrite;
             * these can't contain a `this` or a bare member reference. */
            break;
    }
}

static void rewrite_stmt(AstNode **slot, AstNode *class_decl, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;

    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) {
                rewrite_stmt(&n->list.items[i], class_decl, locals);
            }
            break;
        case AST_IF:
            rewrite_expr(&n->a, class_decl, *locals);
            rewrite_stmt(&n->b, class_decl, locals);
            rewrite_stmt(&n->c, class_decl, locals);
            break;
        case AST_WHILE:
            rewrite_expr(&n->a, class_decl, *locals);
            rewrite_stmt(&n->b, class_decl, locals);
            break;
        case AST_FOR:
            rewrite_stmt(&n->a, class_decl, locals); /* init: var_decl or expr_stmt or NULL */
            rewrite_expr(&n->b, class_decl, *locals); /* cond */
            rewrite_expr(&n->c, class_decl, *locals); /* step */
            rewrite_stmt(&n->d, class_decl, locals);
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
            rewrite_expr(&n->a, class_decl, *locals);
            break;
        case AST_VAR_DECL: {
            rewrite_expr(&n->a, class_decl, *locals); /* initializer, if any */
            LocalVarType *lv = calloc(1, sizeof(LocalVarType)); /* calloc: zero-inits was_reference too */
            lv->name = n->str1;
            lv->type = n->type;
            lv->next = *locals;
            *locals = lv;
            break;
        }
        default:
            /* AST_TYPEDEF_DECL, ... -- nothing to rewrite. */
            break;
    }
}

/* Prepends an explicit "this" parameter (pointer to the owning class) to
 * `method`'s parameter list, then rewrites its body so every `this` and
 * every implicit member reference becomes explicit through that
 * parameter.
 *
 * MUTATES method->list (the parameter list) in place. This is safe for
 * this project's CURRENT pipeline ordering -- sema_run() has already
 * finished (and cached everything it computed, like mangled names and
 * vtable slots, as plain data rather than re-deriving it from the
 * parameter list on demand) before lower_run() ever runs -- but it does
 * mean sema_run() must never be re-invoked on an AST that's already been
 * through this-injection: signature-matching logic like
 * attach_out_of_line's would see the injected "this" parameter and
 * misbehave. Not a concern for main.c's current single-pass pipeline;
 * worth remembering if this project ever grows an incremental/
 * re-analysis mode. */
static void this_inject_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return; /* only definitions have bodies to rewrite */

    AstNode *this_param = ast_new(AST_PARAM, method->line);
    this_param->str1 = strdup("this");
    this_param->type = ast_wrap_pointer(ast_ident(class_decl->str1, method->line), method->line);

    AstList new_params = ast_list_new();
    ast_list_append(&new_params, this_param);
    for (int i = 0; i < method->list.count; i++) {
        ast_list_append(&new_params, method->list.items[i]);
    }
    method->list = new_params;

    /* Seed local tracking with the method's ORIGINAL parameters (index 0
     * of the NEW list is the injected "this" itself, which -- as a
     * plain identifier named "this" -- doesn't need to be in this map at
     * all: nothing will ever look up the name "this" via find_local,
     * since AST_THIS nodes are rewritten directly, not through the
     * AST_IDENT/find_local path). */
    LocalVarType *locals = NULL;
    for (int i = 1; i < method->list.count; i++) {
        AstNode *param = method->list.items[i];
        LocalVarType *lv = calloc(1, sizeof(LocalVarType)); /* calloc: zero-inits was_reference too */
        lv->name = param->str1;
        lv->type = param->type;
        lv->next = locals;
        locals = lv;
    }

    rewrite_stmt(&method->a, class_decl, &locals);
}

static void this_inject_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    this_inject_method(layout->methods.items[j], n);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            this_inject_classes(&n->list);
        }
    }
}

/* ---- phase 3: vtable dispatch codegen (call finalization) --------------
 *
 * Rewrites every call's callee into its FINAL, codegen-ready form:
 *   - A virtual method call becomes `obj->vtable->FIELD(obj, args...)`.
 *   - A non-virtual method call becomes `MangledName(obj, args...)`.
 *   - A free-function call becomes `MangledName(args...)`.
 * In every case the call's own argument list is otherwise untouched;
 * method calls additionally get the object expression PREPENDED as the
 * first argument, matching this-injection's convention that a method's
 * first parameter is the receiver.
 *
 * Driven by sema.c's CallResolution (call->sema_info), NOT by
 * re-resolving names independently -- CallResolution is already
 * overload-aware (arity- and argument-type-matched), where re-deriving
 * "which method is this" via find_member_in_hierarchy alone would only
 * be name-based, a strictly weaker answer. A call sema couldn't resolve
 * (best-effort, per sema.c's own philosophy) is left completely
 * untouched here too -- same reasoning: better to leave it for a human
 * (or a future pass) to notice than to guess.
 *
 * FIELD NAME STABILITY: the vtable struct field name used at a call site
 * must be the SAME regardless of the object's actual runtime type --
 * that's the whole point of a vtable. So the field name always comes
 * from a slot's canonical_method (whichever class ORIGINALLY declared
 * it), never from whichever override CallResolution actually resolved
 * the call to. tests/sample14.cpp exercises this directly: a call
 * inside an OVERRIDING class's own method still uses the BASE class's
 * mangled name as the field name.
 */

/* Finds the vtable slot in `class_decl`'s OWN vtable that currently
 * holds `target` (by pointer identity -- `target`, resolved via
 * CallResolution, IS the exact same AstNode instance stored in whatever
 * class's vtable actually implements it, since sema.c's build_vtable and
 * resolve_call both work over the same ClassLayout/Vtable structures).
 * Sets *canonical_out to the slot's stable, hierarchy-wide field-name
 * source. Returns -1 if not found (shouldn't happen for a genuinely
 * virtual target, but handled defensively rather than assumed). */
static int find_vtable_slot_for_method(AstNode *class_decl, AstNode *target, AstNode **canonical_out) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL || layout->vtable == NULL) return -1;
    for (int i = 0; i < layout->vtable->count; i++) {
        if (layout->vtable->entries[i].method == target) {
            if (canonical_out != NULL) *canonical_out = layout->vtable->entries[i].canonical_method;
            return i;
        }
    }
    return -1;
}

static void prepend_arg(AstList *list, AstNode *arg) {
    AstList new_list = ast_list_new();
    ast_list_append(&new_list, arg);
    for (int i = 0; i < list->count; i++) {
        ast_list_append(&new_list, list->items[i]);
    }
    *list = new_list;
}

/* Wraps `obj_expr` in an explicit cast to `expected_class`'s own pointer
 * type, if its OWN static type (`actual_class`) differs from what the
 * callee actually declared its receiver parameter as. Needed because
 * this project's single-inheritance struct layout guarantees a derived
 * class's fields are a valid PREFIX of its base's (so the memory really
 * is layout-compatible), but C's type system has no way to know that on
 * its own -- passing a `Circle *` where a function expects `Shape *`
 * with no cast is at minimum a warning in standard C, and Vircon32 has
 * already shown itself stricter than that about pointer-type mismatches
 * elsewhere (see docs/DESIGN_NOTES.md). Confirmed against the real
 * compiler that an explicit C-style cast (`(Type *)expr`) is valid
 * Vircon32 syntax, which is what makes this fix possible at all rather
 * than just a documented risk.
 *
 * Returns `obj_expr` UNCHANGED if no cast is needed (same class, or not
 * enough information to know either way -- best-effort, never inserts a
 * cast on a guess). */
static AstNode *cast_receiver_if_needed(AstNode *obj_expr, const AstNode *actual_class,
                                        const AstNode *expected_class) {
    if (expected_class == NULL || actual_class == expected_class) {
        return obj_expr;
    }
    AstNode *cast = ast_new(AST_CAST, obj_expr->line);
    cast->type = ast_wrap_pointer(ast_ident(expected_class->str1, obj_expr->line), obj_expr->line);
    cast->a = obj_expr;
    return cast;
}

/* Wraps `obj_expr` in an explicit address-of (&) if its OWN declared
 * type isn't already a pointer -- needed because a method's receiver
 * parameter is always `ClassName *`, but the object expression a method
 * is called ON isn't always already a pointer. `this` always is (this-
 * injection guarantees it); a `new`-allocated or reference-turned-
 * pointer local always is too -- but a plain, stack-allocated value
 * local (`Player sprite; sprite.setx(320);`) is NOT, and every existing
 * test before Matthew's own hand-written sprite2.cpp happened to only
 * ever exercise the "already a pointer" case, so this gap went entirely
 * unnoticed until a real, deliberately simple test found it: generated
 * code was passing `sprite` (a `struct Player` value) directly where
 * `Player__setx__int` declares `Player *this`, which Vircon32 correctly
 * rejected ("cannot assign struct Player to ... struct Player*").
 *
 * Uses infer_expr_type directly (not resolve_expr_class, which
 * deliberately unwraps pointer/value distinctions away for CLASS-
 * resolution purposes and so can't answer this question at all) --
 * exposed from sema.c specifically for this. */
static AstNode *address_of_if_needed(AstNode *obj_expr, AstNode *class_decl, LocalVarType *locals) {
    AstNode *t = infer_expr_type(obj_expr, class_decl, locals);
    if (t == NULL || t->kind == AST_POINTER_TYPE) {
        /* Already a pointer, OR we couldn't determine its type at all --
         * best-effort, same principle as everywhere else in this file:
         * don't insert a transformation on a guess. Wrongly adding `&`
         * to an expression that's already a pointer would silently
         * produce a double pointer, a strictly worse outcome than
         * leaving the original (already-known) bug in place for
         * whatever rare case reaches this branch. */
        return obj_expr;
    }
    AstNode *addr = ast_new(AST_UNOP, obj_expr->line);
    addr->str1 = strdup("addr"); /* same AST_UNOP shape this-injection/
        codegen already handle elsewhere -- print_unop (codegen.c)
        already knows "addr" means "(&expr)"; no new AST kind needed */
    addr->a = obj_expr;
    return addr;
}

static void finalize_call(AstNode *call, AstNode *class_decl, LocalVarType *locals) {
    CallResolution *cr = (CallResolution *)call->sema_info;
    if (cr == NULL || cr->resolved_target == NULL) {
        return; /* not resolved by sema -- best-effort, leave unlowered */
    }
    AstNode *target = cr->resolved_target;
    AstNode *callee = call->a;
    FuncSemaInfo *target_info = (FuncSemaInfo *)target->sema_info;
    const char *target_mangled = (target_info != NULL) ? target_info->mangled_name : target->str1;

    if (callee->kind == AST_MEMBER) {
        /* A method call -- always explicit `obj->name(...)` by this
         * point, since phase 2 (this-injection) already rewrote every
         * implicit form into this same shape. Note that `obj` here may
         * or may not ALREADY be a pointer -- this-injection only ever
         * guarantees that for `this` itself; an ordinary object
         * expression (a stack-allocated local, say) might not be. */
        AstNode *obj_expr = callee->a;
        AstNode *obj_class = resolve_expr_class(obj_expr, class_decl, locals);
        /* `receiver` is what actually gets used everywhere below --
         * guaranteed to be pointer-typed, unlike `obj_expr` itself. */
        AstNode *receiver = address_of_if_needed(obj_expr, class_decl, locals);

        if (target->ival == 1) {
            /* Virtual: dispatch through the vtable. */
            AstNode *canonical = NULL;
            int slot = (obj_class != NULL) ? find_vtable_slot_for_method(obj_class, target, &canonical) : -1;
            if (slot < 0) {
                return; /* couldn't determine the slot -- best-effort, leave unlowered rather than guess */
            }
            FuncSemaInfo *canonical_info = (canonical != NULL) ? (FuncSemaInfo *)canonical->sema_info : NULL;
            const char *field_name = (canonical_info != NULL) ? canonical_info->mangled_name : target_mangled;

            AstNode *vtable_ref = ast_new(AST_MEMBER, call->line);
            vtable_ref->str1 = strdup("->");
            vtable_ref->str2 = strdup("vtable");
            vtable_ref->a = receiver; /* NOT cast to an ancestor type, deliberately
                -- ->vtable sits at the same offset regardless of static type, and
                every class's OWN vtable struct independently redeclares every
                canonical field name anyway (see emit_vtable_struct in codegen.c),
                so there's no correctness reason to cast for this specific access;
                only the CALL ARGUMENT below needs it. IS, however, address-of'd
                the same as the argument -- `->` genuinely requires a pointer,
                unlike the cast question, which is only about WHICH pointer type */

            AstNode *slot_ref = ast_new(AST_MEMBER, call->line);
            slot_ref->str1 = strdup("->");
            slot_ref->str2 = strdup(field_name);
            slot_ref->a = vtable_ref;

            call->a = slot_ref;

            const AstNode *canonical_class = (obj_class != NULL) ? find_declaring_class(obj_class, canonical) : NULL;
            prepend_arg(&call->list, cast_receiver_if_needed(receiver, obj_class, canonical_class));
        } else {
            /* Non-virtual: direct call to the mangled function. */
            const AstNode *target_class = (obj_class != NULL) ? find_declaring_class(obj_class, target) : NULL;
            AstNode *arg = cast_receiver_if_needed(receiver, obj_class, target_class);
            call->a = ast_ident(target_mangled, call->line);
            prepend_arg(&call->list, arg);
        }
    } else if (callee->kind == AST_IDENT) {
        /* A free-function call -- just finalize the callee to its
         * mangled name; there's no receiver to thread through. */
        call->a = ast_ident(target_mangled, call->line);
    }
    /* else: some other callee shape this project doesn't produce --
     * left untouched. */
}

/* ---- phase 4: operator-overload-to-function-call rewriting -------------
 *
 * As of this round, ALL of the actual RESOLUTION (finding which
 * operatorX overload -- if any -- a BinOp/Assign/Unop/Subscript refers
 * to; member-vs-free precedence; arity/type matching; "no matching
 * overload" diagnostics; access-control enforcement) has moved into
 * sema_run() (see resolve_operator_use in sema.c), for exactly the same
 * reason regular calls have always been resolved there: it gets the SAME
 * diagnostics and access-control treatment a regular call gets, which
 * resolving at lowering time never could (no sema_error() mechanism is
 * reachable from here, and access-control's own pass has already
 * finished by the time lowering runs). See sema.c's resolve_operator_use
 * for the full reasoning; docs/DESIGN_NOTES.md has the postmortem on why
 * this moved (an operator-arity comparison bug that this-injection's
 * timing made real, and would keep making real for anyone who touched
 * this again, versus simply not existing once resolution runs before
 * this-injection ever happens at all).
 *
 * This phase's job is now much smaller: if sema found a match (a
 * CallResolution is already attached to the node's sema_info, complete
 * with an `is_member` flag telling this phase whether to prepend the
 * receiver as an explicit argument), rewrite the node into the
 * equivalent AST_CALL and hand it to finalize_call, reusing every bit of
 * dispatch logic phase 3 already built -- exactly like a normal call,
 * just arriving via different syntax. If sema found nothing, the node is
 * left completely untouched: a plain built-in operation on operands that
 * were never class-typed in the first place (a class-typed operand with
 * no matching operator is now a sema-time ERROR instead, so lowering
 * never even sees that case -- main.c doesn't run lower_run() at all
 * when sema_run() reported any errors).
 */

static void rewrite_operator_use(AstNode **slot, AstNode *lhs_or_operand, AstNode *rhs_or_null,
                                  AstNode *class_decl, LocalVarType *locals) {
    AstNode *n = *slot;
    CallResolution *cr = (CallResolution *)n->sema_info;
    if (cr == NULL || cr->resolved_target == NULL) return; /* sema found no
        match -- leave as a plain built-in operation */

    AstNode *call = ast_new(AST_CALL, n->line);
    if (cr->is_member) {
        AstNode *mem = ast_new(AST_MEMBER, n->line);
        mem->str1 = strdup("->"); /* transient -- finalize_call replaces
            this whole callee wrapper with the real dispatch form below,
            so the exact string here never survives into the final AST */
        mem->str2 = strdup(cr->resolved_target->str1);
        mem->a = lhs_or_operand;
        call->a = mem;
        if (rhs_or_null != NULL) {
            ast_list_append(&call->list, rhs_or_null);
        }
    } else {
        call->a = ast_ident(cr->resolved_target->str1, n->line);
        ast_list_append(&call->list, lhs_or_operand);
        if (rhs_or_null != NULL) {
            ast_list_append(&call->list, rhs_or_null);
        }
    }

    call->sema_info = cr; /* reuse the SAME CallResolution sema already
        built -- finalize_call only ever reads ->resolved_target off of
        it, so sharing it here (rather than allocating a fresh copy) is
        safe and avoids a pointless duplicate allocation */
    *slot = call;
    finalize_call(call, class_decl, locals);
}

static void finalize_calls_expr(AstNode **slot, AstNode *class_decl, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_MEMBER:
            finalize_calls_expr(&n->a, class_decl, locals);
            break;
        case AST_CALL:
            finalize_calls_expr(&n->a, class_decl, locals);
            for (int i = 0; i < n->list.count; i++) {
                finalize_calls_expr(&n->list.items[i], class_decl, locals);
            }
            /* Post-order: arguments (including any nested calls used as
             * arguments) are finalized above before this call itself. */
            finalize_call(n, class_decl, locals);
            break;
        case AST_BINOP:
            finalize_calls_expr(&n->a, class_decl, locals);
            finalize_calls_expr(&n->b, class_decl, locals);
            rewrite_operator_use(slot, n->a, n->b, class_decl, locals);
            break;
        case AST_ASSIGN:
            finalize_calls_expr(&n->a, class_decl, locals);
            finalize_calls_expr(&n->b, class_decl, locals);
            rewrite_operator_use(slot, n->a, n->b, class_decl, locals);
            break;
        case AST_SUBSCRIPT:
            finalize_calls_expr(&n->a, class_decl, locals);
            finalize_calls_expr(&n->b, class_decl, locals);
            rewrite_operator_use(slot, n->a, n->b, class_decl, locals);
            break;
        case AST_UNOP:
            finalize_calls_expr(&n->a, class_decl, locals);
            rewrite_operator_use(slot, n->a, NULL, class_decl, locals);
            break;
        case AST_NEW:
            /* The type being allocated isn't an expression -- only the
             * constructor ARGUMENTS (if any) might contain nested calls/
             * operators needing this same finalization. */
            for (int i = 0; i < n->list.count; i++) {
                finalize_calls_expr(&n->list.items[i], class_decl, locals);
            }
            break;
        case AST_DELETE:
            finalize_calls_expr(&n->a, class_decl, locals);
            break;
        default:
            break;
    }
}

static void finalize_calls_stmt(AstNode **slot, AstNode *class_decl, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) {
                finalize_calls_stmt(&n->list.items[i], class_decl, locals);
            }
            break;
        case AST_IF:
            finalize_calls_expr(&n->a, class_decl, *locals);
            finalize_calls_stmt(&n->b, class_decl, locals);
            finalize_calls_stmt(&n->c, class_decl, locals);
            break;
        case AST_WHILE:
            finalize_calls_expr(&n->a, class_decl, *locals);
            finalize_calls_stmt(&n->b, class_decl, locals);
            break;
        case AST_FOR:
            finalize_calls_stmt(&n->a, class_decl, locals);
            finalize_calls_expr(&n->b, class_decl, *locals);
            finalize_calls_expr(&n->c, class_decl, *locals);
            finalize_calls_stmt(&n->d, class_decl, locals);
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
            finalize_calls_expr(&n->a, class_decl, *locals);
            break;
        case AST_VAR_DECL: {
            finalize_calls_expr(&n->a, class_decl, *locals);
            LocalVarType *lv = calloc(1, sizeof(LocalVarType)); /* calloc: zero-inits was_reference too */
            lv->name = n->str1;
            lv->type = n->type;
            lv->next = *locals;
            *locals = lv;
            break;
        }
        default:
            break;
    }
}

/* Seeds `locals` from `func`'s CURRENT parameter list -- by the time this
 * phase runs, that's already the POST-this-injection list for a method
 * (so index 0 is "this" itself, mapped to its pointer-to-class type,
 * which is exactly what lets a bare `this` reference inside a call's
 * object-expression position resolve correctly via resolve_expr_class). */
static LocalVarType *seed_locals_from_params(AstNode *func) {
    LocalVarType *locals = NULL;
    for (int i = 0; i < func->list.count; i++) {
        AstNode *param = func->list.items[i];
        LocalVarType *lv = calloc(1, sizeof(LocalVarType)); /* calloc: zero-inits was_reference too */
        lv->name = param->str1;
        lv->type = param->type;
        lv->next = locals;
        locals = lv;
    }
    return locals;
}

static void finalize_calls_in_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return;
    LocalVarType *locals = seed_locals_from_params(method);
    finalize_calls_stmt(&method->a, class_decl, &locals);
}

static void finalize_calls_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    finalize_calls_in_method(layout->methods.items[j], n);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            finalize_calls_classes(&n->list);
        }
    }
}

static void finalize_calls_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            finalize_calls_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            /* n->b == NULL: a genuine free function, not an out-of-line
             * method's top-level duplicate (see attach_out_of_line and
             * the identical guard already used elsewhere in this
             * project for exactly this reason). */
            LocalVarType *locals = seed_locals_from_params(n);
            finalize_calls_stmt(&n->a, NULL, &locals);
        }
    }
}

/* ---- phase 5: reference-to-pointer rewriting ----------------------------
 *
 * C has no native reference type, so every parameter/local variable
 * declared with an AST_REFERENCE_TYPE gets it relabeled to
 * AST_POINTER_TYPE. But C++ reference syntax uses `.` for member access
 * (a reference "acts like" the object it refers to), while the now-
 * pointer C representation needs `->` -- so every EXPLICIT `.` access
 * (str1==".") through a bare identifier naming a reference-turned-
 * pointer local/param gets rewritten to `->` too. Compiler-GENERATED
 * member accesses (this-injection's, phase 3's vtable-dispatch chains)
 * are already always "->" by construction and don't need touching here.
 *
 * SCOPE LIMITATION: only tracks reference-ness for bare-identifier
 * locals/parameters, not through a chain (`a.b.c` -- only `a` is checked
 * against the reference-tracking map; whether `.b` or `.c` should ALSO
 * be `->` isn't tracked, since that would require knowing whether `b`
 * itself is a reference-typed MEMBER, a rarer C++ feature this project
 * doesn't otherwise support). Covers the common case (a reference
 * parameter's own members accessed directly) correctly; longer chains
 * through a reference aren't specifically handled.
 *
 * DELIBERATELY A SEPARATE PASS/SEEDING FUNCTION from phase 3's
 * seed_locals_from_params, not a shared one, for a real reason: if
 * reference-detection and type-mutation happened during phase 3's
 * (earlier) seeding, phase 5 re-seeding afterward would see the ALREADY-
 * mutated AST_POINTER_TYPE and incorrectly conclude a parameter was
 * NEVER a reference at all. Running this phase's own, separate
 * detection+mutation together, in one place, after every earlier phase
 * has run and none of them have touched AST_REFERENCE_TYPE at all,
 * avoids that staleness trap entirely.
 */

static LocalVarType *seed_locals_with_reference_tracking(AstNode *func) {
    LocalVarType *locals = NULL;
    for (int i = 0; i < func->list.count; i++) {
        AstNode *param = func->list.items[i];
        LocalVarType *lv = calloc(1, sizeof(LocalVarType));
        lv->name = param->str1;
        lv->was_reference = (param->type != NULL && param->type->kind == AST_REFERENCE_TYPE);
        if (lv->was_reference) {
            param->type->kind = AST_POINTER_TYPE; /* same shape (a=referent), just relabeled */
        }
        lv->type = param->type;
        lv->next = locals;
        locals = lv;
    }
    return locals;
}

static void fix_reference_access_expr(AstNode **slot, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_MEMBER: {
            fix_reference_access_expr(&n->a, locals);
            if (n->str1 != NULL && strcmp(n->str1, ".") == 0 && n->a->kind == AST_IDENT) {
                LocalVarType *lv = find_local(locals, n->a->str1);
                if (lv != NULL && lv->was_reference) {
                    free(n->str1);
                    n->str1 = strdup("->");
                }
            }
            break;
        }
        case AST_CALL:
            fix_reference_access_expr(&n->a, locals);
            for (int i = 0; i < n->list.count; i++) {
                fix_reference_access_expr(&n->list.items[i], locals);
            }
            break;
        case AST_BINOP:
        case AST_ASSIGN:
        case AST_SUBSCRIPT:
            fix_reference_access_expr(&n->a, locals);
            fix_reference_access_expr(&n->b, locals);
            break;
        case AST_UNOP:
        case AST_DELETE:
            fix_reference_access_expr(&n->a, locals);
            break;
        case AST_CAST:
            /* Recurses into the cast's wrapped expression (always just
             * "this" in every case this project currently produces --
             * see finalize_call's cast_receiver_if_needed -- but handled
             * on principle, not just for the cases seen so far, same as
             * every other node kind this walk covers). */
            fix_reference_access_expr(&n->a, locals);
            break;
        case AST_NEW:
            for (int i = 0; i < n->list.count; i++) {
                fix_reference_access_expr(&n->list.items[i], locals);
            }
            break;
        default:
            break;
    }
}

static void fix_reference_access_stmt(AstNode **slot, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) {
                fix_reference_access_stmt(&n->list.items[i], locals);
            }
            break;
        case AST_IF:
            fix_reference_access_expr(&n->a, *locals);
            fix_reference_access_stmt(&n->b, locals);
            fix_reference_access_stmt(&n->c, locals);
            break;
        case AST_WHILE:
            fix_reference_access_expr(&n->a, *locals);
            fix_reference_access_stmt(&n->b, locals);
            break;
        case AST_FOR:
            fix_reference_access_stmt(&n->a, locals);
            fix_reference_access_expr(&n->b, *locals);
            fix_reference_access_expr(&n->c, *locals);
            fix_reference_access_stmt(&n->d, locals);
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
            fix_reference_access_expr(&n->a, *locals);
            break;
        case AST_VAR_DECL: {
            fix_reference_access_expr(&n->a, *locals); /* initializer, if any */
            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = n->str1;
            lv->was_reference = (n->type != NULL && n->type->kind == AST_REFERENCE_TYPE);
            if (lv->was_reference) {
                n->type->kind = AST_POINTER_TYPE;
            }
            lv->type = n->type;
            lv->next = *locals;
            *locals = lv;
            break;
        }
        default:
            break;
    }
}

static void fix_references_in_method(AstNode *method) {
    if (method->kind != AST_FUNC_DEF) return;
    LocalVarType *locals = seed_locals_with_reference_tracking(method);
    fix_reference_access_stmt(&method->a, &locals);
}

static void fix_references_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    fix_references_in_method(layout->methods.items[j]);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            fix_references_classes(&n->list);
        }
    }
}

static void fix_references_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            fix_references_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            LocalVarType *locals = seed_locals_with_reference_tracking(n);
            fix_reference_access_stmt(&n->a, &locals);
        }
    }
}

/* ---- phase 6: new/delete-to-runtime-call rewriting ----------------------
 *
 * Rewrites `new T` / `new T(args)` into a call to a PER-TYPE placeholder
 * allocator (`v32_new_TypeName`, with `args` forwarded to it unchanged),
 * and `delete expr` into a call to a SINGLE, generic placeholder
 * deallocator (`v32_delete`).
 *
 * STILL DELIBERATELY A PLACEHOLDER, not a faithful lowering -- though
 * less of one than it used to be. The grammar gap that used to block
 * this entirely is closed: `NEW type_spec '(' opt_arg_list ')'` is now a
 * real alternative in parser.y, `new T(args)` parses, and sema.c's
 * resolve_new_expr resolves which constructor overload it refers to
 * (with the same arity/type diagnostics a regular call gets -- a
 * genuine mismatch is now a real sema_error(), not silently ignored).
 * What's STILL missing, and still blocks a truly faithful lowering: a
 * real `new T(args)` needs to (a) allocate exactly sizeof(struct T)
 * bytes, which needs a `sizeof` AST representation this project doesn't
 * have, and (b) actually INVOKE the resolved constructor with `args`,
 * which this phase doesn't do -- it forwards `args` to `v32_new_T`
 * unchanged, but nothing about that name or call obligates a future
 * runtime implementation to construct anything; that's still an
 * assumption this phase documents rather than enforces. Both (a) and
 * (b) are tracked as future work for when the actual runtime library and
 * code generator take shape, not silently assumed solved by this phase
 * merely accepting and forwarding arguments now.
 */

/* Finds `class_decl`'s own destructor, if one exists and has a body --
 * same "must have a body" reasoning as find_zero_arg_constructor
 * (phase 7) and the new-side constructor lookups above: calling one
 * that was never emitted would repeat the exact v32_new_Player-shaped
 * mistake. Unlike constructors, there's no ambiguity to resolve here at
 * all -- C++ never allows more than one destructor per class (they
 * can't be overloaded, and take no parameters), so a class either has
 * exactly one or none; no per-overload naming question like `new`'s
 * ever arises for `delete`. A destructor's own AST node is named
 * "~ClassName" (parser.y), not "ClassName" the way a constructor's is
 * -- see sema.c's mangle_free_functions for the same distinction
 * ("dtor" as the mangled name-part rather than the class name). */
static AstNode *find_destructor(AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL) return NULL;
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        if (m->str1 == NULL || m->str1[0] != '~') continue; /* not a destructor */
        if (m->kind != AST_FUNC_DEF) continue; /* no body -- see doc comment above */
        return m;
    }
    return NULL;
}

static void new_delete_rewrite_expr(AstNode **slot, AstNode *class_decl, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_NEW: {
            /* Constructor arguments (if any -- sema.c's resolve_new_expr
             * has already checked their arity/types against whichever
             * constructor overload they resolved to, or left this alone
             * if the type has no declared constructor at all) might
             * themselves contain nested calls/operators/new/delete
             * needing this same rewriting -- post-order, finalize them
             * before building the replacement call. */
            for (int i = 0; i < n->list.count; i++) {
                new_delete_rewrite_expr(&n->list.items[i], class_decl, locals);
            }
            AstNode *cls = type_to_class(n->type);
            const char *type_name = (cls != NULL) ? cls->str1 : "unknown";

            /* Naming: if sema.c's resolve_new_expr resolved a SPECIFIC
             * constructor overload (n->sema_info, a CallResolution),
             * the allocator is named after THAT constructor's own
             * mangled name -- necessary once a class has more than one
             * constructor, since C has no function overloading and a
             * bare "v32_new_TypeName" couldn't otherwise distinguish
             * which overload a given `new T(args)` call site means.
             * Falls back to the bare type name only when no constructor
             * was resolved at all (the class genuinely has none --
             * unambiguous, since there's nothing to disambiguate
             * between). codegen.c's emit_new_delete_runtime uses this
             * exact same logic when deciding what to actually DEFINE --
             * the two have to agree, or the call here would target a
             * name codegen never emits a definition for. */
            CallResolution *cr = (CallResolution *)n->sema_info;
            const char *suffix = type_name; /* fallback: no constructor at all was resolved */
            if (cr != NULL && cr->resolved_target != NULL) {
                /* Uses the resolved constructor's own mangled name
                 * REGARDLESS of whether it has a body -- codegen.c's
                 * emit_new_delete_runtime emits a matching-SIGNATURE
                 * allocator for every constructor a class declares, body
                 * or not (one without a body just allocates, accepting
                 * and discarding the arguments rather than calling
                 * anything -- there's nothing to call). Falling back to
                 * the bare type name for a bodyless constructor would be
                 * wrong the moment that constructor takes any arguments
                 * at all (tests/sample18.cpp's Point(int, int) is
                 * exactly this case) -- the fallback allocator only ever
                 * takes zero arguments, so a 2-argument call site would
                 * target a definition that doesn't accept them. Only
                 * truly falls back to the bare type name when NO
                 * constructor was resolved at all (the class genuinely
                 * has none), which is unambiguous since there's nothing
                 * to disambiguate between. */
                FuncSemaInfo *ctor_info = (FuncSemaInfo *)cr->resolved_target->sema_info;
                if (ctor_info != NULL) suffix = ctor_info->mangled_name;
            }

            size_t len = strlen("v32_new_") + strlen(suffix) + 1;
            char *fn_name = malloc(len);
            snprintf(fn_name, len, "v32_new_%s", suffix);
            AstNode *call = ast_new(AST_CALL, n->line);
            call->a = ast_ident(fn_name, n->line);
            free(fn_name);
            call->list = n->list; /* forward the (already-finalized)
                constructor arguments -- codegen.c's emit_new_delete_runtime
                is what actually allocates and invokes the constructor with
                them now; this phase only decides the SHAPE of the call */
            *slot = call;
            break;
        }
        case AST_DELETE: {
            new_delete_rewrite_expr(&n->a, class_decl, locals);

            /* Naming, mirroring `new`'s own reasoning above but simpler:
             * unlike a constructor, a class can have at most ONE
             * destructor (never overloaded), so there's no per-overload
             * ambiguity to resolve here -- "v32_delete_ClassName" is
             * unambiguous whenever the operand's static class is known
             * at all. codegen.c's emit_delete_runtime uses this exact
             * same naming when deciding what to define -- the two have
             * to agree, same cross-module requirement as `new`'s. Falls
             * back to the untouched, fully generic "v32_delete" (no
             * class suffix -- accepts a bare void*, calls nothing but
             * free()) whenever the operand's class can't be determined
             * at all -- best-effort, matching this project's established
             * philosophy elsewhere rather than guessing. */
            AstNode *obj_class = resolve_expr_class(n->a, class_decl, locals);
            const char *fn_name = "v32_delete";
            char *built_name = NULL;
            if (obj_class != NULL) {
                size_t len = strlen("v32_delete_") + strlen(obj_class->str1) + 1;
                built_name = malloc(len);
                snprintf(built_name, len, "v32_delete_%s", obj_class->str1);
                fn_name = built_name;
            }
            AstNode *call = ast_new(AST_CALL, n->line);
            call->a = ast_ident(fn_name, n->line);
            free(built_name);
            ast_list_append(&call->list, n->a);
            *slot = call;
            break;
        }
        case AST_MEMBER:
            new_delete_rewrite_expr(&n->a, class_decl, locals);
            break;
        case AST_CALL:
            new_delete_rewrite_expr(&n->a, class_decl, locals);
            for (int i = 0; i < n->list.count; i++) {
                new_delete_rewrite_expr(&n->list.items[i], class_decl, locals);
            }
            break;
        case AST_BINOP:
        case AST_ASSIGN:
        case AST_SUBSCRIPT:
            new_delete_rewrite_expr(&n->a, class_decl, locals);
            new_delete_rewrite_expr(&n->b, class_decl, locals);
            break;
        case AST_UNOP:
            new_delete_rewrite_expr(&n->a, class_decl, locals);
            break;
        case AST_CAST:
            new_delete_rewrite_expr(&n->a, class_decl, locals); /* same reasoning as the
                AST_CAST case in fix_reference_access_expr, above */
            break;
        default:
            break;
    }
}

static void new_delete_rewrite_stmt(AstNode **slot, AstNode *class_decl, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) {
                new_delete_rewrite_stmt(&n->list.items[i], class_decl, locals);
            }
            break;
        case AST_IF:
            new_delete_rewrite_expr(&n->a, class_decl, *locals);
            new_delete_rewrite_stmt(&n->b, class_decl, locals);
            new_delete_rewrite_stmt(&n->c, class_decl, locals);
            break;
        case AST_WHILE:
            new_delete_rewrite_expr(&n->a, class_decl, *locals);
            new_delete_rewrite_stmt(&n->b, class_decl, locals);
            break;
        case AST_FOR:
            new_delete_rewrite_stmt(&n->a, class_decl, locals);
            new_delete_rewrite_expr(&n->b, class_decl, *locals);
            new_delete_rewrite_expr(&n->c, class_decl, *locals);
            new_delete_rewrite_stmt(&n->d, class_decl, locals);
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
            new_delete_rewrite_expr(&n->a, class_decl, *locals);
            break;
        case AST_VAR_DECL: {
            new_delete_rewrite_expr(&n->a, class_decl, *locals); /* initializer, e.g. `Foo *f = new Foo;` */
            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = n->str1;
            lv->type = n->type;
            lv->next = *locals;
            *locals = lv;
            break;
        }
        default:
            break;
    }
}

static void new_delete_rewrite_in_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return;
    LocalVarType *locals = seed_locals_from_params(method);
    new_delete_rewrite_stmt(&method->a, class_decl, &locals);
}

static void new_delete_rewrite_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    new_delete_rewrite_in_method(layout->methods.items[j], n);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            new_delete_rewrite_classes(&n->list);
        }
    }
}

static void new_delete_rewrite_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            new_delete_rewrite_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            LocalVarType *locals = seed_locals_from_params(n);
            new_delete_rewrite_stmt(&n->a, NULL, &locals);
        }
    }
}

/* ---- phase 7: constructor invocation for stack-allocated locals --------
 *
 * A plain `ClassName var;` declaration with no explicit initializer
 * should invoke a matching constructor -- this project has never done
 * that at all until now, which a genuinely simple hand-written test
 * (tests/sprite2.cpp is the project's own copy of it) surfaced directly:
 * `Player sprite;` compiled, but never called `Player::Player()`, so
 * `sprite.x`/`sprite.y` were uninitialized stack garbage. See
 * docs/DESIGN_NOTES.md for the full story.
 *
 * SCOPE, DELIBERATELY NARROW FOR THIS FIRST ROUND:
 *
 *   - Only a ZERO-ARGUMENT constructor is matched -- `ClassName var;`
 *     syntax has no way to pass constructor arguments at all (that's
 *     what `ClassName var(args);` or `= ClassName(args)` are for, which
 *     this project doesn't parse as a var_decl initializer form yet).
 *
 *   - Only matched if that constructor HAS A BODY (AST_FUNC_DEF). A
 *     prototype-only constructor (declared, never defined) is
 *     deliberately skipped -- inserting a call to one would produce a
 *     call to a C function that was never emitted, the exact category
 *     of bug `new`'s placeholder allocator already demonstrated
 *     (`v32_new_Player` undeclared). Leaving the declaration
 *     unconstructed in that case is a real, known gap, not silently
 *     "fixed" by calling something that doesn't exist.
 *
 *   - Only a VarDecl appearing directly as a BLOCK statement is
 *     handled -- NOT one appearing as a for-loop's own init clause
 *     (`for (Player p; ...)`). Inserting an extra statement there would
 *     need to land inside the loop BODY instead of right after the
 *     declaration, which is meaningfully more involved for a pattern no
 *     current test uses. Documented gap, not silently mishandled.
 *
 *   - A class WITH virtual methods still gets its constructor called,
 *     but that constructor does NOT populate `this->vtable` -- there's
 *     no static vtable INSTANCE for it to point at yet (the vtable
 *     struct TYPE exists; an actual populated instance of one doesn't).
 *     Calling a virtual method on such an object would still dereference
 *     an uninitialized vtable pointer. This phase makes non-virtual
 *     construction correct; it does NOT make polymorphic objects safe to
 *     use yet.
 *
 * Runs LAST, after every other phase -- the call this phase builds is
 * already in its final, codegen-ready form (a direct call to the
 * constructor's own mangled name, receiver already wrapped in &), so
 * there's nothing for any earlier phase to do to it, and nothing this
 * phase needs any earlier phase to have already done to the surrounding
 * statement list first.
 */

/* Finds `class_decl`'s own zero-argument constructor, if one exists and
 * has a body -- see this phase's own doc comment above for exactly why
 * both conditions matter. Constructors are never inherited in C++, so
 * this only ever needs to check `class_decl`'s OWN methods list, unlike
 * find_declaring_class's ancestor-walking elsewhere in this file. */
static AstNode *find_zero_arg_constructor(AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL) return NULL;
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        if (strcmp(m->str1, class_decl->str1) != 0) continue; /* not a constructor at all */
        if (m->kind != AST_FUNC_DEF) continue; /* no body -- see doc comment */
        if (m->list.count == 1) return m; /* just the injected "this" -- zero explicit params */
    }
    return NULL;
}

static void inject_ctor_calls_stmt(AstNode **slot);

/* Rebuilds `block`'s own statement list, inserting a constructor call
 * immediately after any VarDecl that needs one. Recurses into each
 * statement FIRST (so a nested block's own VarDecls get handled too)
 * before appending it -- and any inserted call -- to the new list. */
static void inject_ctor_calls_block(AstNode *block) {
    AstList new_list = ast_list_new();
    for (int i = 0; i < block->list.count; i++) {
        AstNode *stmt = block->list.items[i];
        inject_ctor_calls_stmt(&stmt);
        ast_list_append(&new_list, stmt);

        if (stmt->kind == AST_VAR_DECL && stmt->a == NULL) {
            AstNode *var_class = type_to_class(stmt->type);
            if (var_class != NULL) {
                AstNode *ctor = find_zero_arg_constructor(var_class);
                if (ctor != NULL) {
                    FuncSemaInfo *info = (FuncSemaInfo *)ctor->sema_info;
                    const char *mangled = (info != NULL) ? info->mangled_name : ctor->str1;

                    AstNode *addr = ast_new(AST_UNOP, stmt->line);
                    addr->str1 = strdup("addr");
                    addr->a = ast_ident(stmt->str1, stmt->line);

                    AstNode *call = ast_new(AST_CALL, stmt->line);
                    call->a = ast_ident(mangled, stmt->line);
                    ast_list_append(&call->list, addr);

                    AstNode *expr_stmt = ast_new(AST_EXPR_STMT, stmt->line);
                    expr_stmt->a = call;

                    ast_list_append(&new_list, expr_stmt);
                }
            }
        }
    }
    block->list = new_list;
}

static void inject_ctor_calls_stmt(AstNode **slot) {
    AstNode *s = *slot;
    if (s == NULL) return;
    switch (s->kind) {
        case AST_BLOCK:
            inject_ctor_calls_block(s);
            break;
        case AST_IF:
            inject_ctor_calls_stmt(&s->b);
            inject_ctor_calls_stmt(&s->c);
            break;
        case AST_WHILE:
            inject_ctor_calls_stmt(&s->b);
            break;
        case AST_FOR:
            /* Deliberately NOT recursing into s->a (the for-loop's own
             * init clause) -- see this phase's own doc comment above. */
            inject_ctor_calls_stmt(&s->d);
            break;
        default:
            break;
    }
}

static void inject_ctor_calls_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (m->kind == AST_FUNC_DEF) inject_ctor_calls_stmt(&m->a);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            inject_ctor_calls_classes(&n->list);
        }
    }
}

static void inject_ctor_calls_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            inject_ctor_calls_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            inject_ctor_calls_stmt(&n->a);
        }
    }
}

/* ---- phase 8: vtable pointer initialization in constructors ------------
 *
 * For every class WITH a vtable, prepends `this->vtable = &ClassName_
 * vtable_instance;` to the very START of each of its OWN constructors
 * that has a body -- BEFORE anything the user actually wrote in that
 * constructor's body, matching real C++'s own timing (the vtable
 * pointer, like base/member subobject initialization, is set up before
 * a constructor's own body runs, so that even code in the body that
 * calls a virtual function dispatches correctly).
 *
 * `ClassName_vtable_instance` is codegen.c's emit_vtable_instance --
 * this phase and that function have to agree on the exact name, the
 * same kind of cross-module naming agreement finalize_call's allocator
 * naming already needs with codegen.c's emit_new_delete_runtime.
 *
 * A constructor with NO body (prototype-only) gets nothing injected --
 * there's no body to inject into, and nothing calls it anyway (phase 7
 * and the `new`-lowering both already skip a bodyless constructor for
 * the identical reason).
 */
static void inject_vtable_init_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL && layout->vtable != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (strcmp(m->str1, n->str1) != 0) continue; /* not a constructor */
                    if (m->kind != AST_FUNC_DEF) continue; /* no body to inject into */

                    AstNode *vtable_ref = ast_new(AST_MEMBER, m->line);
                    vtable_ref->str1 = strdup("->");
                    vtable_ref->str2 = strdup("vtable");
                    vtable_ref->a = ast_ident("this", m->line);

                    size_t len = strlen(n->str1) + strlen("_vtable_instance") + 1;
                    char *instance_name = malloc(len);
                    snprintf(instance_name, len, "%s_vtable_instance", n->str1);
                    AstNode *addr = ast_new(AST_UNOP, m->line);
                    addr->str1 = strdup("addr");
                    addr->a = ast_ident(instance_name, m->line);
                    free(instance_name);

                    AstNode *assign = ast_new(AST_ASSIGN, m->line);
                    assign->str1 = strdup("=");
                    assign->a = vtable_ref;
                    assign->b = addr;

                    AstNode *expr_stmt = ast_new(AST_EXPR_STMT, m->line);
                    expr_stmt->a = assign;

                    AstNode *body = m->a; /* AST_BLOCK */
                    AstList new_list = ast_list_new();
                    ast_list_append(&new_list, expr_stmt);
                    for (int k = 0; k < body->list.count; k++) {
                        ast_list_append(&new_list, body->list.items[k]);
                    }
                    body->list = new_list;
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            inject_vtable_init_classes(&n->list);
        }
    }
}

int lower_run(AstNode *program) {
    compute_struct_layouts(&program->list);
    this_inject_classes(&program->list);
    inject_vtable_init_classes(&program->list);       /* phase 8 -- deliberately
        runs right after this-injection, before anything else touches a
        constructor's body, so the injected statement is simply the FIRST
        thing every later phase (call finalization, etc.) sees */
    finalize_calls_classes(&program->list);       /* phase 3 + phase 4 (operator rewriting lives inside this same walk) */
    finalize_calls_free_functions(&program->list);
    fix_references_classes(&program->list);         /* phase 5 */
    fix_references_free_functions(&program->list);
    new_delete_rewrite_classes(&program->list);      /* phase 6 */
    new_delete_rewrite_free_functions(&program->list);
    inject_ctor_calls_classes(&program->list);        /* phase 7 */
    inject_ctor_calls_free_functions(&program->list);
    return 0;
}

/* ---- dump --------------------------------------------------------------
 *
 * Renders a type in ordinary, human-readable syntax ("int", "Timer *",
 * "v32::Timer") -- deliberately NOT the same rendering as sema.c's
 * type_signature_str, which produces mangled-name-safe fragments
 * ("Timer_ptr", "v32_Timer") for a completely different purpose. Also
 * deliberately NOT typedef-resolved: this shows the type as the source
 * actually wrote it, which is more useful for a human reading this dump
 * than the underlying resolved type would be.
 */

static char *render_type(const AstNode *type) {
    if (type == NULL) return strdup("void");
    switch (type->kind) {
        case AST_IDENT:
            return strdup(type->str1);
        case AST_QUALIFIED_ID: {
            size_t len = 1;
            for (int i = 0; i < type->list.count; i++) {
                len += strlen(type->list.items[i]->str1) + 2; /* +2 for "::" */
            }
            char *out = malloc(len);
            out[0] = '\0';
            for (int i = 0; i < type->list.count; i++) {
                if (i > 0) strcat(out, "::");
                strcat(out, type->list.items[i]->str1);
            }
            return out;
        }
        case AST_POINTER_TYPE:
        case AST_REFERENCE_TYPE: {
            char *inner = render_type(type->a);
            const char *suffix = (type->kind == AST_POINTER_TYPE) ? " *" : " &";
            size_t len = strlen(inner) + strlen(suffix) + 1;
            char *out = malloc(len);
            snprintf(out, len, "%s%s", inner, suffix);
            free(inner);
            return out;
        }
        default:
            return strdup("?");
    }
}

static void indent_line(int indent) {
    for (int i = 0; i < indent; i++) fputs("  ", stdout);
}

static void dump_struct_layout(const AstNode *class_decl, int indent) {
    StructLayout *layout = (StructLayout *)class_decl->lower_info;

    indent_line(indent);
    printf("struct %s {\n", class_decl->str1);

    if (layout == NULL) {
        indent_line(indent + 1);
        printf("(no layout computed)\n");
    } else {
        for (int i = 0; i < layout->count; i++) {
            StructField *f = &layout->fields[i];
            indent_line(indent + 1);
            if (f->kind == FIELD_VTABLE_PTR) {
                printf("[%d] void *vtable;\n", i);
            } else {
                char *type_str = render_type(f->type);
                if (f->declaring_class == class_decl) {
                    printf("[%d] %s %s;\n", i, type_str, f->name);
                } else {
                    printf("[%d] %s %s;  // inherited from %s\n",
                           i, type_str, f->name, f->declaring_class->str1);
                }
                free(type_str);
            }
        }
    }

    indent_line(indent);
    printf("};\n");
}

static void dump_struct_layouts(const AstList *decls, int indent) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            dump_struct_layout(n, indent);
        } else if (n->kind == AST_NAMESPACE_DECL) {
            indent_line(indent);
            printf("namespace %s {\n", n->str1);
            dump_struct_layouts(&n->list, indent + 1);
            indent_line(indent);
            printf("}\n");
        }
    }
}

/* Reuses ast_dump() (the same generic dumper parsing's own output uses)
 * rather than writing a second, parallel printer for method bodies --
 * these ARE just ordinary AstNode trees, now mutated by this-injection;
 * nothing about displaying them needs to be lowering-specific. Only
 * FUNC_DEF methods are shown (prototype-only ones have no body for
 * this-injection to have touched, so there's nothing new to see). */
static void dump_this_injected_methods(const AstList *decls, int indent) {
    for (int i = 0; i < decls->count; i++) {
        const AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (m->kind == AST_FUNC_DEF) {
                        ast_dump(m, indent);
                    }
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            indent_line(indent);
            printf("namespace %s {\n", n->str1);
            dump_this_injected_methods(&n->list, indent + 1);
            indent_line(indent);
            printf("}\n");
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            /* A genuine free function (n->b == NULL excludes an out-of-
             * line method definition's own top-level duplicate -- same
             * guard used everywhere else in this project for the same
             * reason, e.g. sema.c's dump_decls/access_check_free_functions
             * and this file's finalize_calls_free_functions/
             * fix_references_free_functions/new_delete_rewrite_free_
             * functions). THIS BRANCH WAS MISSING ENTIRELY until now --
             * a file with only free functions (no classes at all) showed
             * a completely empty "fully lowered method bodies" section,
             * even though the actual lowering had run correctly; only
             * the DISPLAY was broken. See docs/DESIGN_NOTES.md for the
             * postmortem. */
            ast_dump(n, indent);
        }
    }
}

void lower_dump(const AstNode *program) {
    printf("---- lowering summary (struct layouts) ----\n");
    dump_struct_layouts(&program->list, 0);
    printf("---- lowering summary (fully lowered method bodies: phases 2-7) ----\n");
    dump_this_injected_methods(&program->list, 0);
}
