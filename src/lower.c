#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lower.h"
#include "sema.h"
#include "driver.h" /* g_uses_new_or_delete -- see its own doc comment there */

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
        case AST_CAST:
            /* AST_CAST added here specifically for a USER-WRITTEN cast
             * ("(Base *)this", "(int)member") -- this function
             * previously only ever encountered an AST_CAST as something
             * a LATER lowering phase inserted, never as part of the
             * original, pre-this-injection AST it walks, so this case
             * genuinely didn't need to exist until user-written casts
             * did. The default: case below's own comment ("can't
             * contain a `this` or a bare member reference") is simply
             * false for a cast's own wrapped expression -- confirmed
             * this gap directly by checking, the same way the member-
             * initializer-list argument gap was caught earlier (see
             * this function's own header comment on that). */
            rewrite_expr(&n->a, class_decl, locals);
            break;
        case AST_SIZEOF:
            /* NULL-safe: rewrite_expr's own guard handles the type-
             * taking form's NULL `a` the same way it would any other
             * NULL slot. Only the expression-taking form
             * (`sizeof(this->x)`, say) has anything to rewrite. */
            rewrite_expr(&n->a, class_decl, locals);
            break;
        case AST_TERNARY:
            /* Same reasoning as AST_CAST just above -- a ternary's
             * condition, true-branch, and false-branch can each
             * independently contain a bare `this` or member reference
             * needing this-injection's own rewriting, e.g.
             * `flag ? this->x : y`. All three children, unlike
             * AST_CAST's single one. */
            rewrite_expr(&n->a, class_decl, locals);
            rewrite_expr(&n->b, class_decl, locals);
            rewrite_expr(&n->c, class_decl, locals);
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
            rewrite_expr(&n->a, class_decl, locals); /* array-new's own
                size expression, e.g. `new int[this->count]` -- NULL for
                the ordinary, single-object form, in which case this is
                a harmless no-op (rewrite_expr already returns immediately
                on a NULL slot) */
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
        case AST_LABEL:
            /* The wrapped statement (n->a) needs the exact same
             * this-injection any other statement in this position
             * would get -- a label doesn't change what's inside it. */
            rewrite_stmt(&n->a, class_decl, locals);
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
    AstNode *class_type = ast_ident(class_decl->str1, method->line);
    if (method->str2 != NULL) {
        /* method->str2 == "const" means a trailing `const` was written
         * after this method's own parameter list (see AST_FUNC_DECL's
         * own doc comment in ast.h) -- propagate that to the injected
         * `this` parameter's own type, exactly as real C++'s own
         * compiler would (a const member function's own implicit
         * `this` is `const ClassName *`, not `ClassName *`). This is
         * the actual reason const member functions are worth
         * supporting beyond just parsing the keyword: passing `const`
         * through to the generated C's own `this` parameter is what
         * makes the emitted code's own signature match what the
         * method actually promises, rather than silently discarding
         * the qualifier. Still no ENFORCEMENT that the method body
         * itself honors this (no error for writing through `this`
         * inside a const method) -- matching this project's existing
         * best-effort treatment of `const` everywhere else; a real C
         * compiler downstream, working from the correctly-const-
         * qualified `this` this project now emits, is what would
         * actually catch that violation. */
        class_type = ast_wrap_const(class_type, method->line);
    }
    this_param->type = ast_wrap_pointer(class_type, method->line);

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

    /* A member-initializer list's own argument expressions need the
     * exact same implicit-member-reference rewriting the body itself
     * gets, and for the same reason -- `: y(x * 2)`, where `x` is
     * ANOTHER member (not a constructor parameter), needs `x` rewritten
     * to `this->x` here too, or the generated C references a bare,
     * undeclared identifier no C compiler would accept (C has no
     * implicit struct-field lookup the way this rewriting stands in
     * for). Uses the SAME `locals` (constructor params only, nothing
     * body-local yet -- correct, since a member-init argument can only
     * ever reference a parameter or a member, never a body-local
     * variable, which doesn't exist yet at this point in construction)
     * and runs BEFORE rewrite_stmt below, though order between the two
     * doesn't actually matter -- they touch entirely disjoint parts of
     * the tree (`method->c` vs `method->a`). */
    if (method->c != NULL) {
        for (int i = 0; i < method->c->list.count; i++) {
            AstNode *entry = method->c->list.items[i];
            for (int j = 0; j < entry->list.count; j++) {
                rewrite_expr(&entry->list.items[j], class_decl, locals);
            }
        }
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

    /* Reference-parameter arguments need the exact same "not already a
     * pointer, needs &" treatment address_of_if_needed already gives a
     * method's own receiver just below -- a real, separate gap found
     * after that fix shipped: a C++ reference parameter (`int
     * getArea(Shape &s)`) implicitly takes the address of whatever's
     * passed, but nothing was ever inserting that address-of at the
     * CALL SITE once the parameter itself lowers to a plain C pointer
     * (phase 5, fix_references, only ever rewrites `.` to `->` access
     * INSIDE the callee's own body -- it has no visibility into any
     * call site at all). Confirmed directly, not assumed: a plain
     * pointer parameter (`Shape *s`, called as `getArea(&shape)`)
     * already lowers correctly; only reference parameters were
     * affected. Fixed here, deliberately BEFORE any receiver-
     * prepending below (which would shift call->list's own indices),
     * and deliberately in THIS phase (3+4) rather than phase 5 itself:
     * this runs before phase 5 ever mutates any AST_REFERENCE_TYPE to
     * AST_POINTER_TYPE anywhere in the program, so `target`'s own
     * parameter types are still their true, original shape here
     * regardless of iteration order across functions/classes -- phase
     * 5 mutates them in place, on the SAME shared target node every
     * call site resolves to, so checking this any later would already
     * see every reference relabeled away, with no way left to tell a
     * true `Type *` parameter from a lowered `Type &` one. `this`
     * itself is always excluded automatically here: this-injection
     * (phase 2) already ran before this phase does, so a method's own
     * `target->list` already has its injected `this` as index 0, and
     * `this` is a plain pointer, never a reference, by construction --
     * the loop below only ever walks call->list, the CALLER-visible
     * argument list, and looks up target->list at the matching
     * OFFSET, never at index 0 for a member call, so it can't
     * mismatch `this` against a real argument. */
    {
        int param_offset = (target->list.count > 0 && callee->kind == AST_MEMBER) ? 1 : 0;
        /* An AST_MEMBER callee (an ordinary method call, already
         * rewritten by this-injection into `obj->name(...)` by this
         * point -- OR an operator-overload rewrite that resolved to a
         * member, per rewrite_operator_use above, which already built
         * this same AST_MEMBER shape before calling here) needs the
         * +1 skip, since `target`'s own list already starts with the
         * injected `this`. An AST_IDENT callee (a free-function call,
         * member or not) needs no offset -- target->list already
         * starts at the real first parameter. This matches the exact
         * same callee->kind branching finalize_call already does
         * below, deliberately, not a separate judgment call. */
        for (int i = 0; i < call->list.count; i++) {
            int param_idx = i + param_offset;
            if (param_idx >= target->list.count) break; /* more args than
                declared params -- shouldn't happen for a resolved call,
                but fail closed (stop) rather than read out of bounds */
            AstNode *param = target->list.items[param_idx];
            if (param->type != NULL && param->type->kind == AST_REFERENCE_TYPE) {
                call->list.items[i] = address_of_if_needed(call->list.items[i], class_decl, locals);
            }
        }
    }

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
        case AST_TERNARY:
            /* No rewrite_operator_use call -- same reasoning as
             * check_node's own AST_TERNARY case in sema.c: the ternary
             * operator was never in the overloadable set to begin with,
             * so there's no operator-overload rewriting that could ever
             * apply here. All three children still need their own
             * calls finalized independently, though. */
            finalize_calls_expr(&n->a, class_decl, locals);
            finalize_calls_expr(&n->b, class_decl, locals);
            finalize_calls_expr(&n->c, class_decl, locals);
            break;
        case AST_NEW:
            /* The type being allocated isn't an expression -- only the
             * constructor ARGUMENTS (if any) might contain nested calls/
             * operators needing this same finalization. */
            for (int i = 0; i < n->list.count; i++) {
                finalize_calls_expr(&n->list.items[i], class_decl, locals);
            }
            finalize_calls_expr(&n->a, class_decl, locals); /* array-new's
                own size expression -- e.g. `new int[getCount()]` needs
                THAT call resolved too; NULL for the ordinary,
                single-object form, a harmless no-op in that case */
            break;
        case AST_DELETE:
        case AST_CAST:
            /* AST_CAST added here for the same reason as rewrite_expr's
             * own AST_CAST case above (phase 2) -- a user-written cast's
             * own wrapped expression can contain a call needing this
             * phase's own virtual-dispatch/overload finalization just
             * as much as any other expression can, e.g.
             * "(int)shape->area()". */
            finalize_calls_expr(&n->a, class_decl, locals);
            break;
        case AST_SIZEOF:
            finalize_calls_expr(&n->a, class_decl, locals); /* NULL-safe for the type-taking form, same as AST_CAST's own case just above */
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
        case AST_LABEL:
            finalize_calls_stmt(&n->a, class_decl, locals);
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
        case AST_TERNARY:
            fix_reference_access_expr(&n->a, locals);
            fix_reference_access_expr(&n->b, locals);
            fix_reference_access_expr(&n->c, locals);
            break;
        case AST_CAST:
            /* Recurses into the cast's wrapped expression (always just
             * "this" in every case this project currently produces --
             * see finalize_call's cast_receiver_if_needed -- but handled
             * on principle, not just for the cases seen so far, same as
             * every other node kind this walk covers). */
            fix_reference_access_expr(&n->a, locals);
            break;
        case AST_SIZEOF:
            fix_reference_access_expr(&n->a, locals); /* NULL-safe for the type-taking form, same as AST_CAST's own case just above */
            break;
        case AST_NEW:
            for (int i = 0; i < n->list.count; i++) {
                fix_reference_access_expr(&n->list.items[i], locals);
            }
            fix_reference_access_expr(&n->a, locals); /* array-new's own
                size expression, if any */
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
        case AST_LABEL:
            fix_reference_access_stmt(&n->a, locals);
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

/* NOTE: an earlier draft of this round had a find_destructor() here,
 * mirroring find_zero_arg_constructor (phase 7) and the new-side
 * constructor lookups above. It turned out unnecessary -- this phase's
 * naming scheme is unconditional ("v32_delete_ClassName" regardless of
 * whether a destructor actually exists), and codegen.c's
 * emit_delete_runtime is the only place that actually needs to KNOW
 * whether one exists (to decide whether to emit a call inside that
 * function, or just free()). Left in the first version anyway,
 * genuinely unused, and caught by a real build warning
 * (-Wunused-function) rather than noticed by inspection -- removed
 * here rather than left as dead code nobody asked to keep. */

/* Resolves a type node to the name that goes into a "v32_new..."
 * allocator's own name, for either single-object or array `new`.
 * type_to_class only ever resolves an actual registered CLASS,
 * correctly returning NULL for a primitive type like `int` -- a real
 * bug this project shipped and Matthew's own test run caught directly
 * (tests/sample29.cpp's `new int[5]` produced "v32_new_arr_unknown"
 * instead of "v32_new_arr_int", not caught here first): falling back to
 * the literal string "unknown" instead of the type's own name whenever
 * it wasn't a class. Fixed by falling back to the type node's own name
 * (AST_IDENT's str1) for a bare, non-class type -- `new int`/
 * `new int[N]` are both genuinely legal C++, not just something this
 * project's own classes happen to need. */
static const char *type_name_for_new(AstNode *type) {
    AstNode *cls = type_to_class(type);
    if (cls != NULL) return cls->str1;
    if (type->kind == AST_IDENT) return type->str1;
    return "unknown"; /* some more complex, unhandled type-node shape --
        last resort, not expected to be reached in practice */
}

static void new_delete_rewrite_expr(AstNode **slot, AstNode *class_decl, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_NEW: {
            g_uses_new_or_delete = 1; /* see driver.h's own doc comment on
                this flag for exactly why codegen.c needs it -- a real
                bug otherwise */
            /* Array-new (`new T[N]`) is a COMPLETELY different shape
             * from single-object `new`, handled entirely separately
             * here rather than threaded through the constructor-overload
             * naming logic below: it's allocation-only, deliberately --
             * no per-element construction happens (there's no per-
             * element analogue of phase 7's stack-array support, and no
             * loop-emission machinery exists anywhere in this project
             * yet). Named "v32_new_arr_ClassName" -- distinct from the
             * single-object "v32_new_ClassName..." family, and never
             * ambiguous the way multiple constructor overloads can be,
             * since there's exactly one shape of array-new per class
             * regardless of what constructors it has (none of them get
             * called here at all). codegen.c's emit_array_new_runtime
             * defines it: `malloc(N * sizeof(ClassName))`, cast to the
             * right pointer type, nothing more. */
            if (n->a != NULL) {
                new_delete_rewrite_expr(&n->a, class_decl, locals); /* the size expression itself */
                const char *type_name = type_name_for_new(n->type);
                size_t len = strlen("v32_new_arr_") + strlen(type_name) + 1;
                char *fn_name = malloc(len);
                snprintf(fn_name, len, "v32_new_arr_%s", type_name);
                AstNode *call = ast_new(AST_CALL, n->line);
                call->a = ast_ident(fn_name, n->line);
                free(fn_name);
                ast_list_append(&call->list, n->a);
                *slot = call;
                break;
            }
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
            const char *type_name = type_name_for_new(n->type);

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
            g_uses_new_or_delete = 1;
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
        case AST_TERNARY:
            new_delete_rewrite_expr(&n->a, class_decl, locals);
            new_delete_rewrite_expr(&n->b, class_decl, locals);
            new_delete_rewrite_expr(&n->c, class_decl, locals);
            break;
        case AST_CAST:
            new_delete_rewrite_expr(&n->a, class_decl, locals); /* same reasoning as the
                AST_CAST case in fix_reference_access_expr, above */
            break;
        case AST_SIZEOF:
            new_delete_rewrite_expr(&n->a, class_decl, locals); /* NULL-safe for the type-taking form, same as AST_CAST's own case just above */
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
        case AST_LABEL:
            new_delete_rewrite_stmt(&n->a, class_decl, locals);
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
        case AST_LABEL:
            inject_ctor_calls_stmt(&s->a);
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

/* ---- phase 8a: member-field initializer assignments ---------------------
 *
 * For every constructor whose own member-initializer list has one or
 * more entries resolved as primitive member-field initializers
 * (sema.c's resolve_member_init_list, marked via `entry->ival == 1`),
 * prepends an assignment `this->name = arg;` for each -- in DECLARATION
 * ORDER (the order each field appears in `layout->data_members`, i.e.
 * the order it was actually declared in the class), NOT the order the
 * entries happen to appear in the initializer list itself. This is a
 * well-known, deliberate real-C++ rule -- a member-initializer list's
 * own WRITTEN order has no effect on execution order at all, only
 * declaration order does (the textbook footgun: `Foo(int a) : y(a),
 * x(y) {}` with fields declared `int x; int y;` initializes x BEFORE y,
 * using y's still-uninitialized value, regardless of the list's own
 * left-to-right appearance) -- reproduced here exactly, not the simpler
 * "just use written order" a first pass might reach for.
 *
 * Runs BETWEEN phase 8 (vtable-init) and phase 8b (base-ctor-call) in
 * the pipeline, deliberately -- phase 8, this phase, and phase 8b all
 * PREPEND to the same constructor body, and the phase that prepends
 * LAST ends up FIRST (see phase 8b's own doc comment below for the
 * general reasoning). Call order in lower_run is phase 8, then this
 * phase, then phase 8b, giving a final body order of [base-ctor-call,
 * member-inits (declaration order), vtable-init, ...original body] --
 * matching real C++'s own base-then-members-then-body construction
 * timing for the two orderings that DO have a real-C++ analogue (base
 * before members); this project's own vtable-pointer setup has no
 * precisely analogous point in real C++'s own model to match against,
 * so its position relative to member-inits here is an implementation
 * choice, not a correctness requirement the way base-before-members is.
 */
static void inject_member_init_assigns_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (strcmp(m->str1, n->str1) != 0) continue; /* not a constructor */
                    if (m->kind != AST_FUNC_DEF) continue; /* no body to inject into */
                    if (m->c == NULL) continue; /* no member-init list written at all */

                    /* Walk data_members in DECLARATION order, not m->c's
                     * own written order -- for each declared field, look
                     * for a matching, resolved (ival == 1) entry in m->c.
                     * A field with no matching entry at all (the common
                     * case -- most fields aren't in the list) is simply
                     * skipped, same as real C++ (an un-listed member gets
                     * its default-initialization, which for a primitive
                     * is none at all -- unchanged, pre-existing behavior,
                     * not something this phase needs to do anything
                     * about). */
                    AstList assigns = ast_list_new();
                    for (int d = 0; d < layout->data_members.count; d++) {
                        const char *field_name = layout->data_members.items[d]->str1;
                        AstNode *entry = NULL;
                        for (int k = 0; k < m->c->list.count; k++) {
                            AstNode *e = m->c->list.items[k];
                            if (e->ival == 1 && strcmp(e->str1, field_name) == 0) {
                                entry = e;
                                break;
                            }
                        }
                        if (entry == NULL) continue;

                        AstNode *field_ref = ast_new(AST_MEMBER, m->line);
                        field_ref->str1 = strdup("->");
                        field_ref->str2 = strdup(field_name);
                        field_ref->a = ast_ident("this", m->line);

                        AstNode *assign = ast_new(AST_ASSIGN, m->line);
                        assign->str1 = strdup("=");
                        assign->a = field_ref;
                        assign->b = entry->list.items[0]; /* resolve_member_init_list
                            already confirmed exactly one argument for any
                            ival==1 entry -- see its own doc comment */

                        AstNode *expr_stmt = ast_new(AST_EXPR_STMT, m->line);
                        expr_stmt->a = assign;

                        ast_list_append(&assigns, expr_stmt);
                    }
                    if (assigns.count == 0) continue; /* every entry was
                        base-class delegation (or an error) -- nothing
                        for this phase to do */

                    AstNode *body = m->a; /* AST_BLOCK */
                    AstList new_list = ast_list_new();
                    for (int k = 0; k < assigns.count; k++) {
                        ast_list_append(&new_list, assigns.items[k]);
                    }
                    for (int k = 0; k < body->list.count; k++) {
                        ast_list_append(&new_list, body->list.items[k]);
                    }
                    body->list = new_list;
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            inject_member_init_assigns_classes(&n->list);
        }
    }
}

/* ---- phase 8b: base-class constructor delegation ------------------------
 *
 * For every constructor WITH a member-initializer list that resolved to a
 * base-class delegation (func->c's own entries, each carrying a
 * CallResolution* in its own sema_info once sema.c's
 * resolve_member_init_list has run over it -- see that function's own
 * doc comment for exactly what does and doesn't get one), prepends a
 * direct call to the matched base constructor's own mangled name to the
 * very START of the derived constructor's body -- BEFORE phase 8's own
 * vtable-pointer-init statement, matching real C++'s own timing (a base
 * subobject is fully constructed, its own vtable pointer included,
 * before the derived class's own vtable pointer overwrites it, which in
 * turn happens before the derived constructor's own body runs).
 *
 * Runs immediately AFTER phase 8 (inject_vtable_init_classes) in the
 * pipeline, deliberately -- both phases PREPEND their own statement to
 * the front of the same constructor body, and the phase that prepends
 * LAST ends up FIRST. Phase 8 runs first and prepends the vtable-init;
 * this phase runs right after and prepends the base-ctor call, so the
 * final order is [base-ctor-call, vtable-init, ...original body], not
 * the other way around. (Running this phase before finalize_calls,
 * further down the pipeline, is safe regardless: finalize_call's own
 * first check is `if (cr == NULL || cr->resolved_target == NULL)
 * return;`, and the AST_CALL this phase builds directly never has a
 * sema_info set on it at all -- so even if finalize_calls_classes later
 * walked over it, it would be a guaranteed no-op, the same protection
 * phase 7's own directly-built calls already rely on.)
 *
 * `this` is cast to `Base *` via cast_receiver_if_needed -- safe under
 * this project's own struct-flattening strategy (a base class's fields
 * are always the derived struct's own leading fields, so a `Derived *`
 * and a `Base *` to the same object share a compatible prefix layout),
 * the exact same cast this file already relies on for virtual dispatch
 * through a base-typed pointer and for `delete` through one.
 *
 * Handles BOTH explicit (`: Base(args)`) and IMPLICIT base-class
 * construction -- a later round than this phase's own original,
 * explicit-only version. A constructor with no `: Base(...)` entry at
 * all (either no member-initializer list whatsoever, or one that
 * doesn't mention the base) still gets a call inserted, to the base's
 * own zero-argument constructor, found via the SAME
 * find_zero_arg_constructor this file's own phase 7 already relies on
 * for the analogous "stack-allocated local needs its default
 * constructor called" question -- matching real C++'s own implicit-
 * base-construction rule. If the base has no zero-arg constructor with
 * a body at all, nothing is inserted here -- sema.c's own
 * check_implicit_base_construction has ALREADY run by this point (a
 * sema pass, always completed before lower_run ever starts) and either
 * confirmed this is fine (the base has no constructor of its own at
 * all, so nothing was ever going to be called, matching real C++'s own
 * "implicitly default-constructible" rule for a class with no
 * user-declared constructor) or already reported a real error (the
 * base has SOME constructor, but none callable with zero arguments) --
 * either way, by the time lowering runs, there is nothing left for
 * THIS phase to diagnose, only to act on or correctly skip.
 */
static void inject_base_ctor_calls_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL && layout->base_class_decl != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (strcmp(m->str1, n->str1) != 0) continue; /* not a constructor */
                    if (m->kind != AST_FUNC_DEF) continue; /* no body to inject into */

                    /* Find the base-class-delegation entry, if m->c has
                     * one -- resolve_member_init_list already validated
                     * every entry, so ANY entry that reaches here with a
                     * non-NULL sema_info IS a successfully-resolved
                     * base-class delegation (a member-field entry, or
                     * one that failed to resolve, never gets a
                     * CallResolution attached at all). At most one entry
                     * can ever be base-class delegation -- a class has
                     * at most one direct base, and
                     * resolve_member_init_list only ever resolves an
                     * entry matching THAT base's own name -- so the
                     * first one found is the only one there could be. */
                    AstNode *base_entry = NULL;
                    if (m->c != NULL) {
                        for (int k = 0; k < m->c->list.count; k++) {
                            if (m->c->list.items[k]->sema_info != NULL) {
                                base_entry = m->c->list.items[k];
                                break;
                            }
                        }
                    }

                    const char *mangled;
                    AstList explicit_args = {NULL, 0, 0};
                    if (base_entry != NULL) {
                        /* Explicit delegation. */
                        CallResolution *cr = (CallResolution *)base_entry->sema_info;
                        AstNode *base_ctor = cr->resolved_target;
                        FuncSemaInfo *base_info = (FuncSemaInfo *)base_ctor->sema_info;
                        mangled = (base_info != NULL) ? base_info->mangled_name : base_ctor->str1;
                        explicit_args = base_entry->list;
                    } else {
                        /* Implicit -- no explicit delegation named the
                         * base at all. Find its own zero-arg constructor
                         * (with a body); if none exists, sema.c's own
                         * check_implicit_base_construction has already
                         * either confirmed that's fine (no constructor
                         * at all) or reported the real error (some
                         * constructor, but none zero-arg) -- either way,
                         * nothing left for this phase to do here. */
                        AstNode *implicit_ctor = find_zero_arg_constructor(layout->base_class_decl);
                        if (implicit_ctor == NULL) continue;
                        FuncSemaInfo *implicit_info = (FuncSemaInfo *)implicit_ctor->sema_info;
                        mangled = (implicit_info != NULL) ? implicit_info->mangled_name : implicit_ctor->str1;
                        /* explicit_args stays empty -- a zero-arg call */
                    }

                    AstNode *receiver = cast_receiver_if_needed(ast_ident("this", m->line), n, layout->base_class_decl);

                    AstNode *call = ast_new(AST_CALL, m->line);
                    call->a = ast_ident(mangled, m->line);
                    ast_list_append(&call->list, receiver);
                    for (int k = 0; k < explicit_args.count; k++) {
                        ast_list_append(&call->list, explicit_args.items[k]);
                    }

                    AstNode *expr_stmt = ast_new(AST_EXPR_STMT, m->line);
                    expr_stmt->a = call;

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
            inject_base_ctor_calls_classes(&n->list);
        }
    }
}

/* ---- phase 9: destructor invocation at scope exit -----------------------
 *
 * The mirror of phase 7 (constructor invocation for stack-allocated
 * locals), for teardown instead of construction. A stack-allocated local
 * of class type, whose class has a destructor with a body, now gets that
 * destructor called wherever it goes out of scope -- both the "normal"
 * path (falling off the end of the enclosing block) and an early
 * `return`, in reverse declaration order, matching real C++.
 *
 * WHY THIS IS THE COMPLETE PICTURE, NOT JUST A FIRST SLICE, FOR THIS
 * PROJECT SPECIFICALLY: real C++ RAII also has to handle `break`/
 * `continue`/exceptions unwinding a scope early. This project's grammar
 * has neither `break` nor `continue` at all (confirmed by checking --
 * no BREAK/CONTINUE token, no AST_BREAK/AST_CONTINUE kind, nothing in
 * lexer.l or parser.y), and exceptions are explicitly out of scope for
 * this project entirely. That leaves exactly two ways control can leave
 * a block in the language this project actually accepts: falling off
 * the end, or `return`. Handling both IS the fully general solution
 * here, not a scoped-down approximation of one -- worth stating
 * explicitly rather than leaving it looking like a partial feature.
 *
 * ALGORITHM: walks each block maintaining a stack of per-block
 * "destructible locals" lists (DestructScope, linked to its enclosing
 * scope) -- NOT reusing the existing LocalVarType/`locals` threading
 * used elsewhere in this file, deliberately: that tracking has a known,
 * pre-existing imprecision (a nested block's own locals can leak into
 * an enclosing scope's view when passed through `locals` by address --
 * harmless everywhere it's currently used, since nothing there needed
 * precise block-exit boundaries), and this phase specifically needs
 * exact boundaries to know which destructibles belong to which block.
 * A fresh, purpose-built structure avoids inheriting that imprecision
 * rather than working around it.
 *
 * At a block's own end: appends a destructor call for each of ITS OWN
 * destructibles (not enclosing ones -- those get handled when THEIR
 * block ends), in reverse declaration order.
 *
 * At a `return`: walks the FULL scope chain (this block and every
 * enclosing one, up to the function's top), emitting destructor calls
 * for all of them, innermost-first. A `return expr;` needs special
 * care -- `expr` must be evaluated before any destructor runs (a
 * destructor could depend on or invalidate what the expression reads,
 * and evaluation must precede cleanup regardless), so the return gets
 * rewritten into a small nested block: a temporary holds the
 * already-computed result, the destructor calls run, then a bare
 * `return __v32_ret_tmpN;` uses it. A bare `return;` needs no temporary
 * at all -- the destructor calls simply go before it unchanged.
 *
 * Only a genuinely stack-owned local needs any of this -- a pointer or
 * reference to a class doesn't own what it refers to (same reasoning
 * phase 7 already applies to construction), so only a VarDecl whose OWN
 * type node is a bare class reference (AST_IDENT/AST_QUALIFIED_ID, not
 * POINTER_TYPE/REFERENCE_TYPE/ARRAY_TYPE) is ever a candidate.
 */

typedef struct DestructibleLocal {
    const char *var_name;
    AstNode *dtor; /* the class's own destructor -- always AST_FUNC_DEF
                       (has a body) by construction; see below */
    struct DestructibleLocal *next;
} DestructibleLocal;

typedef struct DestructScope {
    DestructibleLocal *locals; /* this block's own, most-recently-declared first */
    struct DestructScope *parent;
} DestructScope;

/* Same "must have a body" reasoning as every other constructor/
 * destructor lookup in this project -- calling one that was never
 * emitted would repeat the exact v32_new_Player-shaped mistake. */
static AstNode *find_destructor_with_body(AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL) return NULL;
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        if (m->str1 == NULL || m->str1[0] != '~') continue;
        if (m->kind != AST_FUNC_DEF) continue;
        return m;
    }
    return NULL;
}

static AstNode *build_dtor_call_stmt(DestructibleLocal *dl) {
    FuncSemaInfo *dtor_info = (FuncSemaInfo *)dl->dtor->sema_info;
    const char *dtor_mangled = (dtor_info != NULL) ? dtor_info->mangled_name : dl->dtor->str1;

    AstNode *addr = ast_new(AST_UNOP, dl->dtor->line);
    addr->str1 = strdup("addr");
    addr->a = ast_ident(dl->var_name, dl->dtor->line);

    AstNode *call = ast_new(AST_CALL, dl->dtor->line);
    call->a = ast_ident(dtor_mangled, dl->dtor->line);
    ast_list_append(&call->list, addr);

    AstNode *expr_stmt = ast_new(AST_EXPR_STMT, dl->dtor->line);
    expr_stmt->a = call;
    return expr_stmt;
}

static void destruct_scope_block(AstNode *block, DestructScope *parent_scope,
                                  DestructScope *loop_boundary,
                                  DestructScope *break_boundary,
                                  AstNode *func_return_type, int *ret_tmp_counter);

/* Builds and installs, in place of *slot, a small nested block that
 * destroys everything from `scope` up to (but NOT including) `stop_at`,
 * then executes `tail` (the break/continue/return itself, possibly
 * already rewritten -- see the AST_RETURN case's own temporary-variable
 * handling for why a return's `tail` isn't always the original node
 * unchanged). Shared by AST_RETURN (stop_at = NULL, walk the WHOLE
 * scope chain to the function's top) and AST_BREAK/AST_CONTINUE
 * (stop_at = loop_boundary, walk only as far as the loop being exited)
 * -- the two only differ in where the walk stops and what `tail` is,
 * never in the walking/destroying logic itself. */
static void install_destructor_sequence(AstNode **slot, DestructScope *scope,
                                         DestructScope *stop_at, AstNode *tail) {
    int any = 0;
    for (DestructScope *s = scope; s != NULL && s != stop_at && !any; s = s->parent) {
        if (s->locals != NULL) any = 1;
    }
    if (!any) {
        *slot = tail; /* nothing to destroy -- just install the (possibly
            rewritten) tail directly, no wrapping block needed at all */
        return;
    }

    AstList stmts = ast_list_new();
    for (DestructScope *s = scope; s != NULL && s != stop_at; s = s->parent) {
        for (DestructibleLocal *dl = s->locals; dl != NULL; dl = dl->next) {
            ast_list_append(&stmts, build_dtor_call_stmt(dl));
        }
    }
    ast_list_append(&stmts, tail);

    AstNode *replacement = ast_new(AST_BLOCK, tail->line);
    replacement->list = stmts;
    *slot = replacement;
}

static void destruct_scope_stmt(AstNode **slot, DestructScope *scope,
                                 DestructScope *loop_boundary,
                                 DestructScope *break_boundary,
                                 AstNode *func_return_type, int *ret_tmp_counter) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            destruct_scope_block(n, scope, loop_boundary, break_boundary, func_return_type, ret_tmp_counter);
            break;
        case AST_IF:
            /* Both boundaries pass through UNCHANGED -- an `if` doesn't
             * itself introduce a new loop or switch, so a break/continue
             * inside either branch still refers to whatever loop/switch
             * (if any) was already enclosing this `if`. */
            destruct_scope_stmt(&n->b, scope, loop_boundary, break_boundary, func_return_type, ret_tmp_counter);
            destruct_scope_stmt(&n->c, scope, loop_boundary, break_boundary, func_return_type, ret_tmp_counter);
            break;
        case AST_LABEL:
            /* Same "pass through unchanged" treatment as AST_IF just
             * above -- a label doesn't introduce any scope of its own
             * either, it's transparent to destructor-boundary tracking
             * for the statement it wraps. Note this project makes NO
             * attempt to handle a `goto` that jumps INTO or OUT OF a
             * scope with a live destructible local correctly -- see
             * AST_GOTO's own doc comment in ast.h for that explicit,
             * known gap; this case only concerns the label itself
             * being transparent, not goto's own interaction with
             * destruction, which remains unhandled. */
            destruct_scope_stmt(&n->a, scope, loop_boundary, break_boundary, func_return_type, ret_tmp_counter);
            break;
        case AST_WHILE:
            /* The body gets a NEW loop_boundary = scope -- exactly the
             * scope in effect right before entering this loop, i.e. the
             * boundary a break/continue anywhere inside (including
             * nested blocks within the body) should stop at without
             * destroying it or anything further out. break_boundary
             * becomes the SAME new boundary too -- a break directly
             * inside a loop, with no intervening switch, exits that
             * loop, the same target continue already has. (A `switch`
             * nested inside this loop's own body will, in turn, give
             * ITS OWN body a different break_boundary -- see AST_SWITCH
             * below -- without touching loop_boundary at all, so a
             * `continue` inside that nested switch still correctly
             * reaches back out to THIS loop.) */
            destruct_scope_stmt(&n->b, scope, scope, scope, func_return_type, ret_tmp_counter);
            break;
        case AST_FOR:
            /* Same reasoning as AST_WHILE for the body. The init clause
             * (n->a) deliberately keeps the OUTER boundaries unchanged,
             * not new ones -- it runs once, before the loop body's own
             * scope even exists, so it was never "inside" this loop's
             * own boundary to begin with. (A VarDecl in a for-loop's own
             * init clause isn't tracked as a destructible at all
             * regardless -- see this phase's own doc comment in lower.h
             * for that pre-existing, unrelated scope limit; nothing
             * about break/continue changes it.) */
            destruct_scope_stmt(&n->d, scope, scope, scope, func_return_type, ret_tmp_counter);
            break;
        case AST_SWITCH:
            /* Only break_boundary becomes a new boundary (= scope, the
             * same "boundary is the scope the statement itself lives
             * in" pattern AST_WHILE/AST_FOR already use for
             * loop_boundary) -- loop_boundary passes through UNCHANGED.
             * A switch doesn't itself introduce a loop, so `continue`
             * inside one (only valid at all if sema.c already confirmed
             * a loop ALSO encloses this switch) still targets whichever
             * loop was already enclosing it, skipping over the switch
             * entirely -- exactly real C's own rule (`continue` never
             * targets a switch, only `break` does). Reuses
             * destruct_scope_block directly on the AST_SWITCH node
             * itself, not a dedicated walk of its own -- that function
             * only ever reads/writes `block->list`, never anything
             * AST_BLOCK-specific, and a switch's own body (case/default
             * labels interleaved with ordinary statements, in one flat
             * list -- real C's own fall-through structure, not a list
             * of separate per-case containers) is exactly the same
             * shape destruct_scope_block already knows how to walk,
             * tracking destructible locals declared directly in the
             * switch body the same way it would for any other block. */
            destruct_scope_block(n, scope, loop_boundary, scope, func_return_type, ret_tmp_counter);
            break;
        case AST_BREAK:
            /* Uses break_boundary, NOT loop_boundary -- the two differ
             * exactly when a switch is the innermost enclosing
             * construct rather than a loop (see AST_SWITCH above). */
            install_destructor_sequence(slot, scope, break_boundary, n);
            break;
        case AST_CONTINUE:
            /* Always loop_boundary -- continue only ever targets a
             * loop, never a switch, matching real C's own rule; sema.c
             * has already rejected one outside any loop at all, so this
             * should never genuinely be NULL here -- but best-effort or
             * not, install_destructor_sequence handles a NULL stop_at
             * the same way AST_RETURN's own walk already does (walk to
             * the true top), so this doesn't need its own special-cased
             * fallback either. */
            install_destructor_sequence(slot, scope, loop_boundary, n);
            break;
        case AST_RETURN: {
            AstNode *final_return;

            if (n->a != NULL) {
                /* return EXPR; -- EXPR must be evaluated before any
                 * destructor runs; hold its already-computed value in a
                 * temporary rather than risk a destructor invalidating
                 * something the expression depends on. */
                char tmp_name[32];
                snprintf(tmp_name, sizeof(tmp_name), "__v32_ret_tmp%d", (*ret_tmp_counter)++);
                AstNode *tmp_decl = ast_new(AST_VAR_DECL, n->line);
                tmp_decl->str1 = strdup(tmp_name);
                tmp_decl->type = func_return_type; /* shared reference, not
                    deep-copied -- consistent with how this project reuses
                    existing type nodes elsewhere (e.g. infer_expr_type's
                    own AST_NEW case) */
                tmp_decl->a = n->a;

                AstNode *new_return = ast_new(AST_RETURN, n->line);
                new_return->a = ast_ident(tmp_name, n->line);

                /* Unlike AST_BREAK/AST_CONTINUE, a `return expr;` always
                 * needs the temporary-holding VarDecl installed even
                 * when nothing needs destroying -- install_destructor_
                 * sequence's own "nothing to destroy" shortcut would
                 * otherwise drop it. Build the [tmp_decl, ...dtors...,
                 * bare-return] sequence directly here instead of
                 * reusing that helper for this specific case. */
                int any = 0;
                for (DestructScope *s = scope; s != NULL && !any; s = s->parent) {
                    if (s->locals != NULL) any = 1;
                }
                AstList stmts = ast_list_new();
                ast_list_append(&stmts, tmp_decl);
                if (any) {
                    for (DestructScope *s = scope; s != NULL; s = s->parent) {
                        for (DestructibleLocal *dl = s->locals; dl != NULL; dl = dl->next) {
                            ast_list_append(&stmts, build_dtor_call_stmt(dl));
                        }
                    }
                }
                ast_list_append(&stmts, new_return);
                AstNode *replacement = ast_new(AST_BLOCK, n->line);
                replacement->list = stmts;
                *slot = replacement;
                break;
            }

            final_return = n; /* bare `return;` -- reused as-is */
            install_destructor_sequence(slot, scope, NULL, final_return); /* NULL:
                return always walks the FULL chain to the function's own
                top, never stopping at a loop boundary the way
                break/continue does */
            break;
        }
        default:
            break;
    }
}

static void destruct_scope_block(AstNode *block, DestructScope *parent_scope,
                                  DestructScope *loop_boundary,
                                  DestructScope *break_boundary,
                                  AstNode *func_return_type, int *ret_tmp_counter) {
    DestructScope this_scope = { NULL, parent_scope };
    AstList new_list = ast_list_new();

    for (int i = 0; i < block->list.count; i++) {
        AstNode *stmt = block->list.items[i];
        destruct_scope_stmt(&stmt, &this_scope, loop_boundary, break_boundary, func_return_type, ret_tmp_counter);
        ast_list_append(&new_list, stmt);

        if (stmt->kind == AST_VAR_DECL &&
            (stmt->type->kind == AST_IDENT || stmt->type->kind == AST_QUALIFIED_ID)) {
            AstNode *var_class = type_to_class(stmt->type);
            if (var_class != NULL) {
                AstNode *dtor = find_destructor_with_body(var_class);
                if (dtor != NULL) {
                    DestructibleLocal *dl = malloc(sizeof(DestructibleLocal));
                    dl->var_name = stmt->str1;
                    dl->dtor = dtor;
                    dl->next = this_scope.locals;
                    this_scope.locals = dl;
                }
            }
        }
    }

    /* Fall-through exit: this block's own destructibles, reverse
     * declaration order (already the natural order of this_scope.locals,
     * since each was prepended as it was found). For a switch body
     * (this function reused directly on an AST_SWITCH node, not just an
     * AST_BLOCK -- see that case in destruct_scope_stmt), "fall-through"
     * correctly means "control reached the end of the switch body
     * without an explicit break" -- the same real-C behavior whether no
     * case matched at all or the last matching case didn't break,
     * requiring no special handling here beyond what already exists. */
    for (DestructibleLocal *dl = this_scope.locals; dl != NULL; dl = dl->next) {
        ast_list_append(&new_list, build_dtor_call_stmt(dl));
    }

    block->list = new_list;
}

static void destruct_scope_in_method(AstNode *method) {
    if (method->kind != AST_FUNC_DEF) return;
    int ret_tmp_counter = 0;
    destruct_scope_block(method->a, NULL, NULL, NULL, method->type, &ret_tmp_counter);
}

static void destruct_scope_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    destruct_scope_in_method(layout->methods.items[j]);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            destruct_scope_classes(&n->list);
        }
    }
}

static void destruct_scope_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            destruct_scope_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            destruct_scope_in_method(n);
        }
    }
}

/* ---- phase 6a: pointer-type cast insertion for VarDecl initializers ----
 *
 * A real, confirmed bug this closes: `Shape *shapePtr = new Square(4);`
 * -- assigning a derived pointer to a base-typed variable with no
 * explicit cast -- compiles fine in real C++ (an implicit upcast), but
 * Vircon32 C rejects it outright: "types are not compatible: cannot
 * assign struct Square* to struct Shape*", confirmed directly against
 * the real compiler (tests/sample32.cpp, which is what surfaced this).
 * Standard C is stricter than C++ about pointer-type compatibility in
 * exactly this way, and Vircon32 evidently doesn't relax that -- this
 * project's own single-inheritance struct layout guarantees the
 * conversion is actually SAFE (Shape's fields are a literal prefix of
 * Square's), the same reasoning cast_receiver_if_needed already relies
 * on for a method call's own receiver; C's type system just has no way
 * to know that on its own, and here neither does the C code this
 * project emits, until this phase inserts an explicit cast to say so.
 *
 * MUST run BEFORE phase 6 (new/delete rewriting): infer_expr_type
 * needs to see the ORIGINAL `AST_NEW` node to infer "pointer to Square"
 * at all (its own AST_NEW case builds that from `expr->type` directly);
 * once phase 6 has already turned it into a call to
 * `v32_new_Square__Square__int`, there's no NEW node left to ask, just
 * an ordinary function call this project's type inference has no
 * special knowledge of.
 *
 * SCOPE, deliberately narrow, matching how this specific bug was
 * actually found rather than guessing at the full extent of the
 * problem: only a VarDecl's own initializer is covered. The identical
 * mismatch could just as easily arise in a plain assignment
 * (`shapePtr = new Square(4);` after the fact), a function argument, or
 * a return value -- none of those are covered here, a real, documented
 * gap rather than something quietly assumed handled by this phase too.
 */
static void insert_pointer_cast_stmt(AstNode **slot, AstNode *class_decl, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) {
                insert_pointer_cast_stmt(&n->list.items[i], class_decl, locals);
            }
            break;
        case AST_IF:
            insert_pointer_cast_stmt(&n->b, class_decl, locals);
            insert_pointer_cast_stmt(&n->c, class_decl, locals);
            break;
        case AST_LABEL:
            insert_pointer_cast_stmt(&n->a, class_decl, locals);
            break;
        case AST_WHILE:
            insert_pointer_cast_stmt(&n->b, class_decl, locals);
            break;
        case AST_FOR:
            insert_pointer_cast_stmt(&n->a, class_decl, locals);
            insert_pointer_cast_stmt(&n->d, class_decl, locals);
            break;
        case AST_VAR_DECL: {
            if (n->a != NULL && n->type->kind == AST_POINTER_TYPE) {
                AstNode *declared_class = type_to_class(n->type);
                AstNode *init_type = infer_expr_type(n->a, class_decl, *locals);
                if (declared_class != NULL && init_type != NULL && init_type->kind == AST_POINTER_TYPE) {
                    AstNode *init_class = type_to_class(init_type);
                    /* Only when both sides resolve to an actual, KNOWN
                     * class and they genuinely differ -- best-effort,
                     * same philosophy as everywhere else in this file:
                     * if either side can't be resolved at all, don't
                     * guess by inserting a cast that might be wrong. */
                    if (init_class != NULL && init_class != declared_class) {
                        AstNode *cast = ast_new(AST_CAST, n->a->line);
                        cast->type = ast_wrap_pointer(ast_ident(declared_class->str1, n->a->line), n->a->line);
                        cast->a = n->a;
                        n->a = cast;
                    }
                }
            }
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

static void insert_pointer_cast_in_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return;
    LocalVarType *locals = seed_locals_from_params(method);
    insert_pointer_cast_stmt(&method->a, class_decl, &locals);
}

static void insert_pointer_cast_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    insert_pointer_cast_in_method(layout->methods.items[j], n);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            insert_pointer_cast_classes(&n->list);
        }
    }
}

static void insert_pointer_cast_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            insert_pointer_cast_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            insert_pointer_cast_in_method(n, NULL);
        }
    }
}

/* ---- phase 10: ternary-to-if/else rewriting (--target=vircon32 only) ---
 *
 * A real Vircon32-compiler limitation, not a portability nicety: the
 * real Vircon32 C compiler doesn't support the ternary operator at
 * all. This project's own grammar and every earlier lowering phase
 * still treat `cond ? a : b` as an ordinary expression throughout --
 * this phase runs LAST, after everything else that walks an
 * AST_TERNARY node as part of a larger expression (finalize_calls,
 * fix_references, new_delete_rewrite, ...) has already finished, and
 * rewrites the three STATEMENT-level contexts where a ternary
 * commonly appears into equivalent if/else:
 *
 *   int x = cond ? a : b;     ->  int x; if (cond) { x = a; } else { x = b; }
 *   x = cond ? a : b;         ->  if (cond) { x = a; } else { x = b; }
 *   return cond ? a : b;      ->  if (cond) { return a; } else { return b; }
 *
 * Each of these three shapes already has a natural "place to put the
 * value" (the declared variable's own name, the assignment's own
 * lvalue, or a return statement) -- no temporary variable is ever
 * needed, unlike a general ternary-hoisting scheme would require.
 * Chained/nested ternaries (`cond1 ? a : cond2 ? b : c`, a common,
 * idiomatic pattern, not a rare edge case -- confirmed by
 * tests/sample58.cpp's own `classify`, already in this project's
 * suite before this phase existed) are fully handled too: each
 * newly-built branch is recursively re-checked before being wrapped
 * into its own if/else, so a ternary nested arbitrarily deep through
 * a CHAIN of these three shapes keeps unwinding one level at a time
 * until nothing ternary-shaped remains.
 *
 * SCOPE, deliberate and stated plainly: only a ternary that is
 * DIRECTLY the initializer of a var_decl, DIRECTLY the rhs of a plain
 * `=` assignment to a bare identifier, or DIRECTLY a return
 * expression is rewritten -- CHAINED that way (the recursion just
 * above), not nested any other way. A ternary nested INSIDE a call
 * argument, as part of a larger arithmetic expression, inside a
 * for-loop's own init/cond/incr clauses, or assigned through anything
 * other than a bare identifier (`arr[i] = cond ? a : b;`,
 * `obj.field = cond ? a : b;`) -- is left completely untouched and
 * will not compile on the real Vircon32 toolchain. Two real reasons
 * for this boundary, not just running out of time: (1) a fully
 * general rewrite needs to hoist arbitrary sub-expressions into
 * temporaries while preserving evaluation order, sequencing this
 * project doesn't have any machinery for yet (no comma operator,
 * itself a separate, already-tracked gap); (2) the assignment case
 * specifically needed a NARROWER check than "any assignment" even
 * within this scope -- an arbitrary lvalue duplicated across both an
 * if-branch and an else-branch would double-evaluate any side effect
 * inside it (`arr[i++] = cond ? a : b;` would increment `i` in
 * whichever branch runs, but a general lvalue could appear in code
 * generated for BOTH branches if this were done carelessly); a bare
 * identifier has no such risk, so that's the line drawn here.
 *
 * Runs only when g_target == TARGET_VIRCON32 (lower_run's own call
 * site below) -- standard C supports the ternary operator natively,
 * so standard-mode output keeps it exactly as written, unrewritten.
 */

/* True for the three statement shapes this phase rewrites -- kept as
 * its own predicate rather than inlined at each call site, since
 * rewrite_ternary_block (below) needs to check it BEFORE deciding
 * whether a statement gets a 1:1 slot replacement or needs to become
 * TWO statements (var_decl's own case), and getting those two checks
 * out of sync would be a real bug waiting to happen. */
static AstNode *build_ternary_if_else(AstNode *cond, AstNode *then_stmt, AstNode *else_stmt, int line) {
    AstNode *if_node = ast_new(AST_IF, line);
    if_node->a = cond;
    AstNode *then_block = ast_new(AST_BLOCK, line);
    ast_list_append(&then_block->list, then_stmt);
    if_node->b = then_block;
    AstNode *else_block = ast_new(AST_BLOCK, line);
    ast_list_append(&else_block->list, else_stmt);
    if_node->c = else_block;
    return if_node;
}

static void rewrite_ternary_block(AstNode *block);

/* Handles the two REPLACEMENT shapes (assign, return) that can stand
 * in for a single statement slot even OUTSIDE a block's own list --
 * `if (cond) return x ? a : b;` (no braces) is valid in this grammar,
 * and the rewritten if/else is still exactly one statement, so it
 * fits in that same slot with no list to insert into needed. var_decl
 * is deliberately NOT handled here -- its own rewrite needs to become
 * TWO statements (the now-uninitialized declaration, then the
 * if/else), which only rewrite_ternary_block's own list-splicing can
 * do; a var_decl reaching this function (as a bare if/while/for body
 * with no surrounding block) is left unrewritten, the same documented
 * boundary this whole phase's own doc comment already states for
 * anything a plain slot-replacement can't safely reach. */
static void rewrite_ternary_stmt(AstNode **slot) {
    AstNode *s = *slot;
    if (s == NULL) return;
    switch (s->kind) {
        case AST_BLOCK:
            rewrite_ternary_block(s);
            break;
        case AST_IF:
            rewrite_ternary_stmt(&s->b);
            rewrite_ternary_stmt(&s->c);
            break;
        case AST_WHILE:
            rewrite_ternary_stmt(&s->b);
            break;
        case AST_FOR:
            rewrite_ternary_stmt(&s->d);
            break;
        case AST_LABEL:
            rewrite_ternary_stmt(&s->a);
            break;
        case AST_RETURN:
            if (s->a != NULL && s->a->kind == AST_TERNARY) {
                AstNode *t = s->a;
                AstNode *then_ret = ast_new(AST_RETURN, s->line);
                then_ret->a = t->b;
                AstNode *else_ret = ast_new(AST_RETURN, s->line);
                else_ret->a = t->c;
                /* Recurse on each newly-built branch BEFORE wrapping it
                 * into the if/else below -- a chained/nested ternary
                 * (`cond1 ? a : cond2 ? b : c`, a common, idiomatic
                 * pattern, not a rare edge case) means t->b or t->c can
                 * itself be another AST_TERNARY; without this, only
                 * the OUTERMOST level would get rewritten, leaving a
                 * still-broken, still-uncompilable ternary sitting
                 * inside the else-branch this function just built.
                 * Each recursive call sees the exact same AST_RETURN
                 * shape this case already handles, so it either
                 * rewrites it again (another nested ternary) or does
                 * nothing (a plain expression, the base case). */
                rewrite_ternary_stmt(&then_ret);
                rewrite_ternary_stmt(&else_ret);
                *slot = build_ternary_if_else(t->a, then_ret, else_ret, s->line);
            }
            break;
        case AST_EXPR_STMT:
            if (s->a != NULL && s->a->kind == AST_ASSIGN
                && s->a->str1 != NULL && strcmp(s->a->str1, "=") == 0
                && s->a->a != NULL && s->a->a->kind == AST_IDENT
                && s->a->b != NULL && s->a->b->kind == AST_TERNARY) {
                AstNode *assign = s->a;
                AstNode *t = assign->b;
                const char *name = assign->a->str1;

                AstNode *then_assign = ast_new(AST_ASSIGN, s->line);
                then_assign->str1 = strdup("=");
                then_assign->a = ast_ident(name, s->line);
                then_assign->b = t->b;
                AstNode *then_stmt = ast_new(AST_EXPR_STMT, s->line);
                then_stmt->a = then_assign;

                AstNode *else_assign = ast_new(AST_ASSIGN, s->line);
                else_assign->str1 = strdup("=");
                else_assign->a = ast_ident(name, s->line);
                else_assign->b = t->c;
                AstNode *else_stmt = ast_new(AST_EXPR_STMT, s->line);
                else_stmt->a = else_assign;

                /* Same chained-ternary recursion as AST_RETURN just
                 * above, same reasoning -- t->b/t->c can themselves be
                 * another AST_TERNARY, and each recursive call sees
                 * the exact AST_EXPR_STMT(AST_ASSIGN(...)) shape this
                 * case already knows how to rewrite. */
                rewrite_ternary_stmt(&then_stmt);
                rewrite_ternary_stmt(&else_stmt);
                *slot = build_ternary_if_else(t->a, then_stmt, else_stmt, s->line);
            }
            break;
        default:
            break;
    }
}

static void rewrite_ternary_block(AstNode *block) {
    AstList new_list = ast_list_new();
    for (int i = 0; i < block->list.count; i++) {
        AstNode *stmt = block->list.items[i];

        if (stmt->kind == AST_VAR_DECL && stmt->a != NULL && stmt->a->kind == AST_TERNARY) {
            /* The one shape needing TWO statements in its place --
             * see this whole phase's own doc comment above for why
             * this can only happen here, inside a real list, never
             * via rewrite_ternary_stmt's own single-slot replacement. */
            AstNode *t = stmt->a;
            AstNode *decl_only = stmt;
            decl_only->a = NULL; /* same node, reused -- just drops its own initializer */
            ast_list_append(&new_list, decl_only);

            AstNode *then_assign = ast_new(AST_ASSIGN, stmt->line);
            then_assign->str1 = strdup("=");
            then_assign->a = ast_ident(stmt->str1, stmt->line);
            then_assign->b = t->b;
            AstNode *then_stmt = ast_new(AST_EXPR_STMT, stmt->line);
            then_stmt->a = then_assign;

            AstNode *else_assign = ast_new(AST_ASSIGN, stmt->line);
            else_assign->str1 = strdup("=");
            else_assign->a = ast_ident(stmt->str1, stmt->line);
            else_assign->b = t->c;
            AstNode *else_stmt = ast_new(AST_EXPR_STMT, stmt->line);
            else_stmt->a = else_assign;

            /* Same chained-ternary recursion as rewrite_ternary_stmt's
             * own AST_RETURN/AST_EXPR_STMT cases, same reasoning: t->b
             * or t->c can itself be another AST_TERNARY
             * (`int x = cond1 ? a : cond2 ? b : c;`), and each
             * recursive call sees the exact AST_EXPR_STMT(AST_ASSIGN
             * (...)) shape rewrite_ternary_stmt already knows how to
             * rewrite. */
            rewrite_ternary_stmt(&then_stmt);
            rewrite_ternary_stmt(&else_stmt);

            ast_list_append(&new_list, build_ternary_if_else(t->a, then_stmt, else_stmt, stmt->line));
        } else {
            rewrite_ternary_stmt(&stmt);
            ast_list_append(&new_list, stmt);
        }
    }
    block->list = new_list;
}

static void rewrite_ternary_in_method(AstNode *method) {
    if (method->kind != AST_FUNC_DEF) return;
    rewrite_ternary_stmt(&method->a);
}

static void rewrite_ternary_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    rewrite_ternary_in_method(layout->methods.items[j]);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            rewrite_ternary_classes(&n->list);
        }
    }
}

static void rewrite_ternary_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            rewrite_ternary_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            rewrite_ternary_in_method(n);
        }
    }
}

/* ---- word-size check for parameters and return values (vircon32 mode only) ---
 *
 * Matthew's own confirmation of the real Vircon32 C compiler's own
 * limitation: a function's parameters and return value must each be
 * EXACTLY one word (32 bits). No by-value struct, union, or array
 * larger than that is accepted at all -- a pointer must be used
 * instead. Vircon32's own primitive types make the size computation
 * genuinely simple, not something needing careful arithmetic: `int`,
 * `float`, and every pointer are already exactly one word, and even
 * `char`/`short`/`double`/etc (recent-compiler aliases, per Matthew)
 * are "mere syntactic sugar" over the same 4-byte word underneath --
 * so EVERY field, of ANY of this project's supported primitive
 * types, is exactly one word, with no per-type size table to build or
 * get wrong. A class/struct's own total size in words is therefore
 * just its StructLayout's own field count (compute_struct_layouts,
 * just above -- data members plus a vtable pointer, if any, each
 * counted once, each exactly one word).
 *
 * SCOPE, stated plainly: only catches a BARE (not pointer, not
 * reference -- const-qualified still counts, since `const Shape` is
 * still passed/returned by value) class or struct type whose own
 * StructLayout has more than one field. This is a LOWER BOUND, not an
 * exact byte count -- an array-typed or nested-struct-typed DATA
 * MEMBER only ever contributes ONE to its own StructLayout's field
 * count here, even though it may itself be multiple words wide, so a
 * struct with exactly one such field could still under-count as
 * "one word" when it's actually more. This is the same conservative
 * direction as every other best-effort check in this project: better
 * to miss a genuine violation than to warn on code that's actually
 * fine. Unions are deliberately NOT checked here at all -- a union's
 * own members overlap rather than stack, so an ordinary union of
 * simple primitive members is already exactly one word by
 * construction, regardless of how many members it has; only a union
 * containing an array or nested-struct member large enough to itself
 * exceed one word would violate this, a narrower case not worth this
 * round's own scope.
 *
 * Vircon32 mode only (checked at each call site below, not gated
 * once at the top of this section) -- standard C has no such
 * restriction at all, matching this project's own established
 * "vircon32 mode is the one needing the extra treatment" shape for
 * every other entry in docs/VIRCON32_QUIRKS.md.
 */

static AstNode *bare_class_type(const AstNode *type) {
    /* Unwraps ONLY a const qualifier, never a pointer or reference --
     * this function exists specifically to identify a BY-VALUE class/
     * struct type, so a pointer or reference to one (already exactly
     * one word, matching every other Vircon32-safe value) must NOT
     * match here. Returns the class's own AST_CLASS_DECL, or NULL if
     * `type` isn't a bare class/struct reference at all (a primitive,
     * a pointer, a reference, or an unresolvable name). */
    while (type != NULL && type->kind == AST_CONST_TYPE) {
        type = type->a;
    }
    if (type == NULL || (type->kind != AST_IDENT && type->kind != AST_QUALIFIED_ID)) {
        return NULL;
    }
    return type_to_class(type);
}

static void warn_if_multiword_by_value(const AstNode *type, int line, const char *context) {
    if (g_target != TARGET_VIRCON32) return;
    AstNode *class_decl = bare_class_type(type);
    if (class_decl == NULL) return;
    StructLayout *layout = (StructLayout *)class_decl->lower_info;
    if (layout != NULL && layout->count > 1) {
        sema_warning(line,
                     "'%s' has type '%s', a %d-word struct/class passed or "
                     "returned BY VALUE -- the real Vircon32 C compiler only "
                     "supports parameters and return values that are exactly "
                     "one word (int, float, or a pointer); pass or return a "
                     "pointer to '%s' instead",
                     context, class_decl->str1, layout->count, class_decl->str1);
    }
}

static void check_word_size_in_func(AstNode *func) {
    warn_if_multiword_by_value(func->type, func->line, "return value");
    for (int i = 0; i < func->list.count; i++) {
        AstNode *param = func->list.items[i];
        warn_if_multiword_by_value(param->type, param->line, param->str1);
    }
}

static void check_word_sizes_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    check_word_size_in_func(layout->methods.items[j]);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            check_word_sizes_classes(&n->list);
        }
    }
}

static void check_word_sizes_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            check_word_sizes_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            /* Same "free function, not an out-of-line method
             * definition" test access_check_free_functions (sema.c)
             * already uses -- an out-of-line method's own n->b holds
             * its Class:: qualifier; a free function's is always
             * NULL. Out-of-line methods are already covered via
             * check_word_sizes_classes's own walk of layout->methods,
             * which already includes them -- this avoids checking the
             * same function's own signature twice. */
            check_word_size_in_func(n);
        }
    }
}

int lower_run(AstNode *program) {
    compute_struct_layouts(&program->list);
    check_word_sizes_classes(&program->list); /* reads StructLayout,
        just computed -- must run after compute_struct_layouts, but
        before anything mutates a parameter's or return type's own
        AST_REFERENCE_TYPE (fix_references, phase 5) since a bare
        class/struct check would otherwise need to account for that
        mutation too; running this early, right after the layouts it
        depends on exist, sidesteps the question entirely */
    check_word_sizes_free_functions(&program->list);
    this_inject_classes(&program->list);
    inject_vtable_init_classes(&program->list);       /* phase 8 -- deliberately
        runs right after this-injection, before anything else touches a
        constructor's body, so the injected statement is simply the FIRST
        thing every later phase (call finalization, etc.) sees */
    inject_member_init_assigns_classes(&program->list); /* phase 8a --
        MUST run between phase 8 and phase 8b (not before phase 8, not
        after phase 8b) -- see this phase's own doc comment for the full
        prepend-ordering reasoning behind the final body order this
        produces */
    inject_base_ctor_calls_classes(&program->list);    /* phase 8b -- MUST
        run right after phase 8, not before it: both PREPEND to the same
        constructor body, and the phase that prepends LAST ends up FIRST
        -- see this phase's own doc comment for the full ordering
        reasoning (base-ctor-call needs to end up ahead of vtable-init in
        the final body, matching real C++'s own construction order) */
    finalize_calls_classes(&program->list);       /* phase 3 + phase 4 (operator rewriting lives inside this same walk) */
    finalize_calls_free_functions(&program->list);
    fix_references_classes(&program->list);         /* phase 5 */
    fix_references_free_functions(&program->list);
    insert_pointer_cast_classes(&program->list);      /* phase 6a -- MUST run
        before phase 6 below, while AST_NEW nodes are still intact; see
        this phase's own doc comment for why */
    insert_pointer_cast_free_functions(&program->list);
    new_delete_rewrite_classes(&program->list);      /* phase 6 */
    new_delete_rewrite_free_functions(&program->list);
    inject_ctor_calls_classes(&program->list);        /* phase 7 */
    inject_ctor_calls_free_functions(&program->list);
    destruct_scope_classes(&program->list);           /* phase 9 */
    destruct_scope_free_functions(&program->list);
    if (g_target == TARGET_VIRCON32) {
        /* phase 10 -- deliberately conditional, unlike every earlier
         * phase: standard C supports the ternary operator natively,
         * so standard-mode output keeps `cond ? a : b` exactly as
         * written. See this phase's own doc comment (just above) for
         * the full reasoning and the real, stated scope boundary on
         * which ternary-containing statements this actually rewrites. */
        rewrite_ternary_classes(&program->list);
        rewrite_ternary_free_functions(&program->list);
    }
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
        case AST_CONST_TYPE: {
            /* Unlike type_signature_str's own AST_CONST_TYPE case
             * (sema.c), which deliberately makes const TRANSPARENT for
             * name-mangling purposes, this function is for human-
             * readable -vvv display (the "lowering summary (struct
             * layouts)" dump) -- here, showing "const" is the more
             * useful, accurate rendering, matching what the field was
             * actually declared as, not hiding it the way mangling
             * needs to for a different reason entirely. */
            char *inner = render_type(type->a);
            size_t len = strlen(inner) + 7; /* "const " + inner + NUL */
            char *out = malloc(len);
            snprintf(out, len, "const %s", inner);
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
    printf("---- lowering summary (fully lowered method bodies: phases 2-9) ----\n");
    dump_this_injected_methods(&program->list, 0);
}
