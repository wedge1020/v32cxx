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
        default:
            /* Literals, AST_QUALIFIED_ID, AST_NEW, ... -- nothing to
             * rewrite; these can't contain a `this` or a bare member
             * reference. */
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
         * implicit form into this same shape. */
        AstNode *obj_expr = callee->a;

        if (target->ival == 1) {
            /* Virtual: dispatch through the vtable. */
            AstNode *obj_class = resolve_expr_class(obj_expr, class_decl, locals);
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
            vtable_ref->a = obj_expr;

            AstNode *slot_ref = ast_new(AST_MEMBER, call->line);
            slot_ref->str1 = strdup("->");
            slot_ref->str2 = strdup(field_name);
            slot_ref->a = vtable_ref;

            call->a = slot_ref;
        } else {
            /* Non-virtual: direct call to the mangled function. */
            call->a = ast_ident(target_mangled, call->line);
        }

        prepend_arg(&call->list, obj_expr);
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
 * Until now, operator-overload SUPPORT in this project has only ever
 * covered DECLARING an operator (`Vector2D operator+(Vector2D other)`,
 * parsed and mangled like any other method since the operator-overload-
 * syntax round). Nothing has ever resolved USING one -- an ordinary
 * `a + b` in source is just an AST_BINOP node, with no indication
 * anywhere that it might mean a call to `a`'s `operator+`. This phase is
 * the first place that gap gets closed: it looks for a matching operator
 * overload (member first, respecting name-hiding via
 * find_member_in_hierarchy; a free function as a fallback, matching how
 * real C++ allows binary operators to be declared either way) and, if
 * found, rewrites the BinOp/Assign/Unop/Subscript node into the
 * equivalent AST_CALL -- then hands it straight to finalize_call, reusing
 * every bit of dispatch logic (virtual/non-virtual/free) phase 3 already
 * built, rather than duplicating any of it.
 *
 * KNOWN GAP, asymmetric with regular calls: a normal call gets resolved
 * (and, on failure, diagnosed) during sema_run(), with a real "no
 * matching overload" error and full access-control enforcement. Operator
 * USE resolution happens here instead, at LOWERING time, after sema has
 * already finished. That means: no error if a class-typed operand's
 * types/arity genuinely don't match any declared operator -- the
 * expression is just silently left as a plain built-in BinOp/Assign/
 * Unop/Subscript, which downstream codegen would then treat as a native
 * C operation on operands it doesn't actually support; and no access-
 * control check for a private/protected operator invoked from somewhere
 * that shouldn't be allowed to. Both real, and both flagged rather than
 * silently present -- moving this resolution earlier, into sema_run()
 * itself, is future work.
 */

static const char *binop_operator_name(const char *op) {
    if (strcmp(op, "+") == 0) return "operator+";
    if (strcmp(op, "-") == 0) return "operator-";
    if (strcmp(op, "*") == 0) return "operator*";
    if (strcmp(op, "/") == 0) return "operator/";
    if (strcmp(op, "==") == 0) return "operator==";
    if (strcmp(op, "!=") == 0) return "operator!=";
    if (strcmp(op, "<") == 0) return "operator<";
    if (strcmp(op, ">") == 0) return "operator>";
    if (strcmp(op, "<=") == 0) return "operator<=";
    if (strcmp(op, ">=") == 0) return "operator>=";
    return NULL; /* &&/|| aren't in this project's supported operator_symbol
                  * list at all (see parser.y), so they're never overloadable
                  * here -- always a plain built-in BinOp. */
}

static const char *assign_operator_name(const char *op) {
    if (strcmp(op, "=") == 0) return "operator=";
    if (strcmp(op, "+=") == 0) return "operator+=";
    if (strcmp(op, "-=") == 0) return "operator-=";
    if (strcmp(op, "*=") == 0) return "operator*=";
    if (strcmp(op, "/=") == 0) return "operator/=";
    return NULL;
}

static const char *unop_operator_name(const char *op) {
    /* Only "neg" (unary minus) and "!" correspond to operators this
     * project's grammar actually supports overloading (see
     * operator_symbol in parser.y) -- "~", "addr", "deref", and the
     * pre/post ++/-- forms have no overload syntax to have matched, so
     * they're never rewritten here, always plain built-in AST_UNOP. */
    if (strcmp(op, "neg") == 0) return "operator-";
    if (strcmp(op, "!") == 0) return "operator!";
    return NULL;
}

/* Looks for a matching operator overload for `op_name` given
 * `lhs_or_operand` (and, for a binary operator, `rhs_or_null`) and, if
 * found, replaces *slot (currently a BinOp/Assign/Unop/Subscript node)
 * with the equivalent AST_CALL, immediately finalized via finalize_call.
 * If nothing matches, *slot is left completely untouched. */
/* A method's list.count includes the injected "this" parameter ONLY if
 * this-injection has already run on it -- which only ever happens for
 * AST_FUNC_DEF (a body to rewrite); this_inject_method returns
 * immediately for anything that isn't (see phase 2), so a prototype-only
 * AST_FUNC_DECL's list.count still reflects exactly what was written,
 * no "this" counted in it. Comparing a raw list.count against an
 * expected EXPLICIT-parameter count is therefore off by one for any
 * member operator that has a body and correct (by coincidence, not
 * design) for one that doesn't -- exactly the shape of a real bug this
 * project shipped once already (tests/sample15.cpp: operator+ and
 * operator== both have out-of-line bodies and were silently skipped;
 * operator[] has no body in that test and happened to work). This
 * computes the count that's ACTUALLY comparable against a written
 * arity, regardless of which case applies. */
static int effective_param_count(const AstNode *method) {
    if (method->kind == AST_FUNC_DEF) {
        return method->list.count - 1; /* subtract the injected "this" */
    }
    return method->list.count;
}

static void resolve_operator_use(AstNode **slot, const char *op_name, AstNode *lhs_or_operand,
                                  AstNode *rhs_or_null, AstNode *class_decl, LocalVarType *locals) {
    if (op_name == NULL) return;

    AstNode *obj_class = resolve_expr_class(lhs_or_operand, class_decl, locals);
    AstNode *target = NULL;
    int is_member = 0;

    if (obj_class != NULL) {
        AstNode *owner = NULL;
        AstNode *member = find_member_in_hierarchy(obj_class, op_name, &owner);
        int expected_params = (rhs_or_null != NULL) ? 1 : 0; /* member operators take
            the OTHER operand explicitly; `this` supplies the left-hand one implicitly */
        if (member != NULL && effective_param_count(member) == expected_params) {
            target = member;
            is_member = 1;
        }
    }

    if (target == NULL) {
        AstNode **candidates = NULL;
        int count = 0, cap = 0;
        collect_free_function_candidates(op_name, &candidates, &count, &cap);
        int expected_params = (rhs_or_null != NULL) ? 2 : 1; /* a free operator takes
            BOTH operands explicitly -- there's no implicit `this` at all */
        for (int i = 0; i < count; i++) {
            if (candidates[i]->list.count == expected_params) {
                target = candidates[i];
                break;
            }
        }
        free(candidates);
    }

    if (target == NULL) {
        return; /* no matching operator declared anywhere applicable --
                    leave this as a plain built-in operation */
    }

    AstNode *call = ast_new(AST_CALL, (*slot)->line);
    if (is_member) {
        AstNode *mem = ast_new(AST_MEMBER, (*slot)->line);
        mem->str1 = strdup("->"); /* transient -- finalize_call replaces
            this whole callee wrapper with the real dispatch form below,
            so the exact string here never survives into the final AST */
        mem->str2 = strdup(op_name);
        mem->a = lhs_or_operand;
        call->a = mem;
        if (rhs_or_null != NULL) {
            ast_list_append(&call->list, rhs_or_null);
        }
    } else {
        call->a = ast_ident(op_name, (*slot)->line);
        ast_list_append(&call->list, lhs_or_operand);
        if (rhs_or_null != NULL) {
            ast_list_append(&call->list, rhs_or_null);
        }
    }

    CallResolution *cr = calloc(1, sizeof(CallResolution));
    cr->resolved_target = target;
    call->sema_info = cr;

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
            resolve_operator_use(slot, binop_operator_name(n->str1), n->a, n->b, class_decl, locals);
            break;
        case AST_ASSIGN:
            finalize_calls_expr(&n->a, class_decl, locals);
            finalize_calls_expr(&n->b, class_decl, locals);
            resolve_operator_use(slot, assign_operator_name(n->str1), n->a, n->b, class_decl, locals);
            break;
        case AST_SUBSCRIPT:
            finalize_calls_expr(&n->a, class_decl, locals);
            finalize_calls_expr(&n->b, class_decl, locals);
            resolve_operator_use(slot, "operator[]", n->a, n->b, class_decl, locals);
            break;
        case AST_UNOP:
            finalize_calls_expr(&n->a, class_decl, locals);
            resolve_operator_use(slot, unop_operator_name(n->str1), n->a, NULL, class_decl, locals);
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
 * Rewrites `new T` into a call to a PER-TYPE placeholder allocator
 * (`v32_new_TypeName`), and `delete expr` into a call to a SINGLE,
 * generic placeholder deallocator (`v32_delete`).
 *
 * DELIBERATELY A PLACEHOLDER, not a faithful lowering. A real `new T`
 * needs to (a) allocate exactly sizeof(struct T) bytes and (b) invoke
 * T's constructor with whatever arguments were written. Neither is
 * possible here yet: (a) needs a `sizeof` AST representation this
 * project doesn't have; (b) needs constructor ARGUMENTS to be parseable
 * in a `new` expression at all, which they currently aren't --
 * `unary_expr: NEW type_spec` in parser.y only ever captured a bare
 * type name, never `(args)`. That's a genuine grammar gap that predates
 * this phase and blocks a faithful lowering; this phase doesn't work
 * around it, it just makes sure the AST has SOME concrete, C-shaped call
 * expression here rather than an un-lowerable AST_NEW/AST_DELETE node
 * surviving into codegen. The actual runtime library behind
 * `v32_new_*`/`v32_delete`, and fixing the grammar to support
 * constructor arguments, are both tracked as future work, not silently
 * assumed solved.
 */

static void new_delete_rewrite_expr(AstNode **slot) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_NEW: {
            AstNode *cls = type_to_class(n->type);
            const char *type_name = (cls != NULL) ? cls->str1 : "unknown";
            size_t len = strlen("v32_new_") + strlen(type_name) + 1;
            char *fn_name = malloc(len);
            snprintf(fn_name, len, "v32_new_%s", type_name);
            AstNode *call = ast_new(AST_CALL, n->line);
            call->a = ast_ident(fn_name, n->line);
            free(fn_name);
            *slot = call;
            break;
        }
        case AST_DELETE: {
            new_delete_rewrite_expr(&n->a);
            AstNode *call = ast_new(AST_CALL, n->line);
            call->a = ast_ident("v32_delete", n->line);
            ast_list_append(&call->list, n->a);
            *slot = call;
            break;
        }
        case AST_MEMBER:
            new_delete_rewrite_expr(&n->a);
            break;
        case AST_CALL:
            new_delete_rewrite_expr(&n->a);
            for (int i = 0; i < n->list.count; i++) {
                new_delete_rewrite_expr(&n->list.items[i]);
            }
            break;
        case AST_BINOP:
        case AST_ASSIGN:
        case AST_SUBSCRIPT:
            new_delete_rewrite_expr(&n->a);
            new_delete_rewrite_expr(&n->b);
            break;
        case AST_UNOP:
            new_delete_rewrite_expr(&n->a);
            break;
        default:
            break;
    }
}

static void new_delete_rewrite_stmt(AstNode **slot) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) {
                new_delete_rewrite_stmt(&n->list.items[i]);
            }
            break;
        case AST_IF:
            new_delete_rewrite_expr(&n->a);
            new_delete_rewrite_stmt(&n->b);
            new_delete_rewrite_stmt(&n->c);
            break;
        case AST_WHILE:
            new_delete_rewrite_expr(&n->a);
            new_delete_rewrite_stmt(&n->b);
            break;
        case AST_FOR:
            new_delete_rewrite_stmt(&n->a);
            new_delete_rewrite_expr(&n->b);
            new_delete_rewrite_expr(&n->c);
            new_delete_rewrite_stmt(&n->d);
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
            new_delete_rewrite_expr(&n->a);
            break;
        case AST_VAR_DECL:
            new_delete_rewrite_expr(&n->a); /* initializer, e.g. `Foo *f = new Foo;` */
            break;
        default:
            break;
    }
}

static void new_delete_rewrite_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (m->kind == AST_FUNC_DEF) new_delete_rewrite_stmt(&m->a);
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
            new_delete_rewrite_stmt(&n->a);
        }
    }
}

int lower_run(AstNode *program) {
    compute_struct_layouts(&program->list);
    this_inject_classes(&program->list);
    finalize_calls_classes(&program->list);       /* phase 3 + phase 4 (operator rewriting lives inside this same walk) */
    finalize_calls_free_functions(&program->list);
    fix_references_classes(&program->list);         /* phase 5 */
    fix_references_free_functions(&program->list);
    new_delete_rewrite_classes(&program->list);      /* phase 6 */
    new_delete_rewrite_free_functions(&program->list);
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
    printf("---- lowering summary (fully lowered method bodies: phases 2-6) ----\n");
    dump_this_injected_methods(&program->list, 0);
}
