#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lower.h"
#include "sema.h"
#include "driver.h" /* g_uses_new_or_delete -- see its own doc comment there */

/* ---- quirk-rewrite notes log --------------------------------------------
 *
 * A handful of lowering phases exist ONLY because Vircon32's C compiler
 * (or, for the ternary case, its lexer) rejects something ordinary,
 * standards-conforming C would accept outright -- an implicit upcast, a
 * bare function-name-as-value, a const-discarding assignment, a `?:`
 * expression anywhere but the three "direct" shapes. Each of those
 * phases silently rewrites the AST to route around the quirk; from the
 * generated output alone there is no way to tell that a rewrite even
 * happened, let alone why. This log exists purely to surface that at
 * `-vvv`: every site that inserts one of these quirk-driven rewrites
 * calls lower_note() with a one-line, human-readable description (source
 * line number included), and main.c prints the whole log after
 * lower_dump() whenever verbosity >= 3. Best-effort and diagnostic only
 * -- nothing here is read by codegen or by any other phase, so a dropped
 * or truncated note (the fixed-size array below silently stops
 * accepting new notes past its capacity) never changes what the
 * transpiler actually emits, only what it reports about itself. */
#define LOWER_NOTES_MAX 512
static char *g_lower_notes[LOWER_NOTES_MAX];
static int g_lower_notes_count = 0;

static void lower_notes_reset(void) {
    for (int i = 0; i < g_lower_notes_count; i++) {
        free(g_lower_notes[i]);
        g_lower_notes[i] = NULL;
    }
    g_lower_notes_count = 0;
}

static void lower_note(int line, const char *fmt, ...) {
    if (g_lower_notes_count >= LOWER_NOTES_MAX) return; /* best-effort --
        see doc comment above; silently drop rather than grow unbounded */
    char buf[512];
    int prefix_len = snprintf(buf, sizeof(buf), "line %d: ", line);
    if (prefix_len < 0) prefix_len = 0;
    if ((size_t)prefix_len < sizeof(buf)) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf + prefix_len, sizeof(buf) - (size_t)prefix_len, fmt, args);
        va_end(args);
    }
    g_lower_notes[g_lower_notes_count] = strdup(buf);
    g_lower_notes_count++;
}

/* Called from main.c at verbosity >= 3, right after lower_dump(). Reset
 * happens at the top of lower_run() itself (not here), so the log always
 * reflects the single most recent lowering pass. */
void lower_notes_print(void) {
    printf("---- lowering notes (quirk-driven rewrites) ----\n");
    if (g_lower_notes_count == 0) {
        printf("(none -- no quirk-driven rewrite fired for this program)\n");
        return;
    }
    for (int i = 0; i < g_lower_notes_count; i++) {
        printf("%s\n", g_lower_notes[i]);
    }
}

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
        case AST_DIRECT_INIT:
            /* Same reasoning as AST_NEW just below -- `Shape shape(x,
             * this->y);`'s own constructor arguments can just as easily
             * contain a bare `this` or implicit member reference. */
            for (int i = 0; i < n->list.count; i++) {
                rewrite_expr(&n->list.items[i], class_decl, locals);
            }
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
        case AST_ASM:
            /* Nothing to do: an asm body is opaque string literals --
             * no `this`, member references, or references to rewrite.
             * Passed through to codegen.c verbatim. */
            break;
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

/* True if `type` denotes a const-qualified object the way a receiver's
 * own declared type would carry it in this project's grammar -- a bare
 * AST_CONST_TYPE, or one level through AST_REFERENCE_TYPE/
 * AST_POINTER_TYPE (`const Shape &`, `const Shape *`), matching exactly
 * the wrapper shape this_inject_method builds for a const method's own
 * injected `this` parameter (see its own comment, above) and the shape
 * a `const T &`/`const T *` parameter already has as written. Doesn't
 * chase typedefs -- a receiver's own declared type is never itself a
 * typedef name anywhere this project's own machinery produces one. */
static int receiver_type_is_const(const AstNode *type) {
    if (type == NULL) return 0;
    if (type->kind == AST_CONST_TYPE) return 1;
    if ((type->kind == AST_REFERENCE_TYPE || type->kind == AST_POINTER_TYPE)
        && type->a != NULL && type->a->kind == AST_CONST_TYPE) {
        return 1;
    }
    return 0;
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
 * `needs_const_strip` is a SECOND, independent reason to cast, found
 * only by an actual Vircon32 compiler run (not gcc): calling a
 * non-const method through a const object (`const Shape &s` calling
 * `s.area()`, where `area()` isn't itself declared `const`) forwards a
 * `const Shape *` receiver into a method whose own injected `this` is
 * plain `Shape *` -- real C++ would refuse to even COMPILE this (a
 * const reference can't call a non-const method), but this project
 * deliberately doesn't enforce const-correctness anywhere (see the
 * README's own stated scope boundary) and gcc only ever WARNED about
 * it (`-Wdiscarded-qualifiers`); the real Vircon32 C compiler makes it
 * a hard error instead ("cannot assign const struct Shape* to struct
 * Shape*: discards const qualifier"). Since this project has chosen
 * not to enforce const-correctness, the honest fix is to make the
 * permitted-but-unchecked case actually COMPILE, the same way a caller
 * writing an explicit `const_cast` would in real C++ -- not to start
 * rejecting code this project has never rejected before. Caller
 * computes `needs_const_strip` from the object's own inferred type
 * AND the target's own receiver-parameter type together (a const
 * object calling a method that's ALSO const needs no cast at all --
 * both sides already agree).
 *
 * Returns `obj_expr` UNCHANGED only when NEITHER reason applies (same
 * class AND no const to strip, or not enough information to know
 * either way -- best-effort, never inserts a cast on a guess). */
static AstNode *cast_receiver_if_needed(AstNode *obj_expr, const AstNode *actual_class,
                                        const AstNode *expected_class, int needs_const_strip) {
    int class_mismatch = (expected_class != NULL && actual_class != expected_class);
    if (!class_mismatch && !needs_const_strip) {
        return obj_expr; /* neither reason to cast applies */
    }
    const AstNode *cast_class = (expected_class != NULL) ? expected_class : actual_class;
    if (cast_class == NULL) {
        return obj_expr; /* needs_const_strip but no class known to cast
            to at all -- best-effort, same "never cast on a guess"
            principle as everywhere else in this file; shouldn't happen
            in practice since actual_class is always known whenever a
            call resolved to a member at all */
    }
    AstNode *cast = ast_new(AST_CAST, obj_expr->line);
    cast->type = ast_wrap_pointer(ast_ident(cast_class->str1, obj_expr->line), obj_expr->line);
    cast->a = obj_expr;
    if (needs_const_strip && !class_mismatch) {
        lower_note(obj_expr->line, "inserted (%s *) cast to strip a const "
            "receiver -- Vircon32 rejects passing a const object to a "
            "non-const method outright (\"discards const qualifier\"), and "
            "this project doesn't enforce const-correctness of its own", cast_class->str1);
    } else if (class_mismatch) {
        lower_note(obj_expr->line, "inserted (%s *) receiver cast for a base/"
            "derived method call%s", cast_class->str1,
            needs_const_strip ? " (also stripping const)" : "");
    }
    return cast;
}

/* Wraps a member-access OR subscript READ (`other->size`, `text[i]`) in
 * an explicit cast to the value's own (unqualified) type, when the value
 * is being read THROUGH a const-qualified receiver -- see the original
 * doc comment above for the real-Vircon32-compiler bug class this
 * closes. The AST_SUBSCRIPT arm is new: `text[i]` on a `const char *`
 * parameter is the same "const propagates through a by-value read"
 * quirk on a different expression shape (surfaced by a real program's
 * drawText(video, "...") forwarding text[i] into an int parameter).
 * Still deliberately narrow: only bare AST_MEMBER / AST_SUBSCRIPT
 * values at read positions (assignment RHS, call argument, return,
 * initializer) -- never an lvalue, never `&`-operand. */
static AstNode *strip_const_member_read(AstNode *expr, AstNode *class_decl, LocalVarType *locals) {
    if (expr == NULL) return expr;
    if (expr->kind != AST_MEMBER && expr->kind != AST_SUBSCRIPT) return expr;
    AstNode *obj_type = infer_expr_type(expr->a, class_decl, locals);
    if (!receiver_type_is_const(obj_type)) return expr;
    AstNode *read_type = infer_expr_type(expr, class_decl, locals);
    if (read_type == NULL) return expr;
    /* Strip one const wrapper if the inferred type carries one (the
     * subscript path can: `const char *`'s element type infers as
     * `const char`). A no-op for the member path when the member's
     * declared type was already unqualified. */
    if (read_type->kind == AST_CONST_TYPE) read_type = read_type->a;
    if (read_type == NULL) return expr;
    AstNode *cast = ast_new(AST_CAST, expr->line);
    cast->type = read_type; /* reused by reference, not deep-copied --
        same convention as every other cast built in this file */
    cast->a = expr;
    const char *type_name = (read_type->kind == AST_IDENT && read_type->str1 != NULL)
        ? read_type->str1 : "value";
    lower_note(expr->line, "inserted a (%s) cast around a const %s read "
        "-- Vircon32 rejects assigning a const-qualified value into a "
        "plain one outright (\"discards const qualifier\"), unlike gcc, "
        "which only ever warns, and this project doesn't enforce "
        "const-correctness of its own", type_name,
        expr->kind == AST_SUBSCRIPT ? "subscript" : "member");
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
    if (t == NULL || t->kind == AST_POINTER_TYPE || t->kind == AST_REFERENCE_TYPE) {
        /* AST_REFERENCE_TYPE here (not just AST_POINTER_TYPE) is a real,
         * separate fix, not part of the original pointer check: this
         * whole phase (3/4) deliberately runs BEFORE phase 5 relabels
         * any AST_REFERENCE_TYPE to AST_POINTER_TYPE anywhere (see
         * finalize_call's own comment on that ordering, and
         * inject_reference_return_address_stmt's, both above) -- so a
         * bare identifier that is itself a reference PARAMETER or LOCAL
         * still reports AST_REFERENCE_TYPE here via infer_expr_type
         * (which returns the declared type as-is, unmodified), even
         * though it is ALREADY going to be a pointer value by the time
         * phase 5 finishes and codegen ever sees it. Without this,
         * forwarding a reference parameter as another reference-typed
         * argument (e.g. a free function `addThem(const Vector2D &a,
         * const Vector2D &b)` computing `a + b`, which resolves to
         * `operator+(const Vector2D &other)`) got a WRONGLY-inserted
         * extra `&`, producing `(&a)` where `a` is already pointer-
         * valued -- a real double-pointer type mismatch, caught by
         * actually compiling tests/sample15.cpp's generated C (gcc
         * reported `const struct Vector2D **` passed where `struct
         * Vector2D *` was expected), not guessed at. Confirmed this is
         * safe for the ORIGINAL "value local needs &" case too: a
         * plain, non-reference local's own declared type is never
         * AST_REFERENCE_TYPE in the first place, so this added
         * condition only ever fires for exactly the already-pointer-
         * bound-for case it's meant to.
         *
         * Already a pointer, OR we couldn't determine its type at all --
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

/* Reference- AND plain-pointer-parameter ARGUMENTS need the same
 * explicit base-class pointer cast a method's own RECEIVER already
 * gets from cast_receiver_if_needed (above) -- a real, separate gap
 * found via a real user's Space Invaders program: `mPlayer.
 * collidesWith(*mBombs[i])` resolves (after the type_matches_param fix
 * in sema.c that allows a derived-class object to bind to a base-class
 * reference parameter, matching real C++'s own reference-binding
 * rules) to `collidesWith(const Entity &e)` called with a `Bullet`
 * argument -- but nothing at the LOWERING level ever inserted the
 * pointer cast the resulting C code actually needs. `&bulletObj` is a
 * `struct Bullet *`, and passing that where a `const struct Entity *`
 * parameter is declared is a real pointer-type mismatch in plain C
 * (confirmed via gcc: `-Wincompatible-pointer-types`); this project's
 * own established pattern (see cast_receiver_if_needed's own doc
 * comment, and docs/DESIGN_NOTES.md) is that the real Vircon32 compiler
 * is likely to reject outright what gcc only warns about, so this is
 * worth closing even though gcc itself doesn't hard-fail on it.
 *
 * A PLAIN pointer parameter (`void push(Animal *a)` called with a
 * `Cat *`) has the exact same gap for an entirely different reason:
 * sema.c's own resolve_overload_generic deliberately skips
 * type_matches_param altogether whenever a name has only ONE
 * candidate at all (no real overload ambiguity to resolve -- see its
 * own doc comment), so a plain-pointer argument's class is never even
 * CHECKED against the declared parameter type, let alone cast --
 * confirmed directly by a real gcc run showing the identical
 * `-Wincompatible-pointer-types` warning for exactly this shape.
 * Handled together here since both need the identical cast once a
 * mismatch is found; only the caller-side treatment of the argument
 * differs (a reference parameter needs address-of'd first, a plain
 * pointer parameter's argument is already pointer-valued as written).
 *
 * `arg` must already be pointer-valued by the time this runs -- for a
 * reference parameter, called AFTER address_of_if_needed (mirroring
 * the order cast_receiver_if_needed is applied relative to
 * address_of_if_needed for a method's own receiver); for a plain
 * pointer parameter, the argument already is pointer-valued as
 * written, so no address-of step is needed first. `arg_static_type` is
 * the argument's ORIGINAL (pre-&, for the reference case) static type,
 * used only to recover its class. `param_type` is the parameter's
 * declared (pre-lowering) type -- an AST_REFERENCE_TYPE or
 * AST_POINTER_TYPE, possibly wrapping AST_CONST_TYPE.
 *
 * Deliberately trusts sema.c's own type_matches_param (for the
 * reference-parameter case) or the "only one candidate, no type check
 * needed" latitude resolve_overload_generic already takes (for the
 * plain-pointer case) rather than reverifying the classes are actually
 * related (same-or-descendant) itself -- is_same_or_descendant is
 * static to sema.c and not worth exposing a second time just to
 * recheck what amounts to the same discipline cast_receiver_if_needed
 * already trusts for a receiver. Returns `arg` unchanged whenever the
 * classes already match, or either class can't be determined -- same
 * "never insert a cast on a guess" discipline as everywhere else in
 * this file. */
static AstNode *cast_ref_arg_if_needed(AstNode *arg, AstNode *arg_static_type,
                                       AstNode *param_type) {
    if (param_type == NULL) return arg;
    if (param_type->kind != AST_REFERENCE_TYPE && param_type->kind != AST_POINTER_TYPE) {
        return arg;
    }
    AstNode *param_class = type_to_class(param_type);
    AstNode *arg_class = type_to_class(arg_static_type);
    if (param_class == NULL || arg_class == NULL || param_class == arg_class) {
        return arg; /* same class already, or not enough information known */
    }
    int param_is_const = receiver_type_is_const(param_type);
    AstNode *class_ident = ast_ident(param_class->str1, arg->line);
    AstNode *pointee = param_is_const ? ast_wrap_const(class_ident, arg->line) : class_ident;
    AstNode *cast = ast_new(AST_CAST, arg->line);
    cast->type = ast_wrap_pointer(pointee, arg->line);
    cast->a = arg;
    lower_note(arg->line, "inserted (%s%s *) cast for a base/derived "
        "reference-parameter argument", param_is_const ? "const " : "",
        param_class->str1);
    return cast;
}

/* Forward declarations -- both are defined further down this file
 * (clone_default_expr and fill_default_args, right before
 * finalize_call's own definition), but fixup_ctor_reference_args
 * (just below) needs to call fill_default_args, and C requires a
 * declaration before use. See fill_default_args's own doc comment,
 * at its actual definition, for what these do and why. */
static AstNode *clone_default_expr(const AstNode *n);
static void fill_default_args(AstList *call_args, AstNode *target, int param_offset);

/* A real, previously-undiscovered bug found alongside the copy-
 * constructor overload-matching fix in sema.c (type_matches_param):
 * once a copy constructor (`Shape(const Shape &other);`) actually
 * resolved at all, its argument still didn't get the implicit `&` a
 * reference parameter needs -- `Shape c(a);`/`new Shape(a)` both
 * transpiled `a` completely unchanged, passing a bare `struct Shape`
 * value where the lowered `const struct Shape *` parameter expects a
 * pointer (confirmed directly against real gcc output: "incompatible
 * type for argument... expected 'const struct Shape *' but argument is
 * of type 'struct Shape'"). finalize_call (above) already solves this
 * EXACT problem for an ordinary function/method call's own arguments --
 * but a constructor invoked via `new T(args)` or this project's own
 * direct-initialization syntax never goes through finalize_call at all
 * (constructor calls are built directly by new_delete_rewrite_expr's
 * AST_NEW case and inject_ctor_calls_block's AST_DIRECT_INIT case,
 * neither of which touches finalize_call's machinery), so neither one
 * ever got this fix. Factored out here so both call sites share
 * exactly one implementation rather than reimplementing the same
 * offset-by-`this` reference-parameter walk twice.
 *
 * `ctor`'s own parameter list always starts with an injected `this`
 * (phase 2, this-injection, already ran on every constructor same as
 * every other method) -- `args` (the constructor CALL's own argument
 * list) never includes it, so every index needs the same +1 offset
 * finalize_call's own AST_MEMBER-callee branch already applies for the
 * identical reason. */
static void fixup_ctor_reference_args(AstList *args, AstNode *ctor, AstNode *class_decl, LocalVarType *locals) {
    int param_offset = (ctor->list.count > 0) ? 1 : 0;
    /* Same default-argument splice finalize_call's own AST_MEMBER/
     * AST_IDENT call path applies (see fill_default_args's own doc
     * comment) -- constructors need it too (`Random(unsigned int seed
     * = 0x1234ABCDu)` is itself a constructor), and this is the one
     * shared choke point both AST_NEW and AST_DIRECT_INIT already run
     * their constructor calls through, so fixing it here covers both
     * without duplicating the call. Deliberately before the reference-
     * argument loop just below, same ordering reason as finalize_call's
     * own call to fill_default_args. `fill_default_args` itself is
     * defined further up this file, just above this function's own
     * forward-declared use of it here -- both were factored out
     * together since they solve the same "every call needs every
     * argument explicit" constraint for the same two constructor call
     * shapes (forward-declared just above this function; its real
     * definition, and clone_default_expr's, sit further down this
     * file, right before finalize_call). */
    fill_default_args(args, ctor, param_offset);
    for (int i = 0; i < args->count; i++) {
        int param_idx = i + param_offset;
        if (param_idx >= ctor->list.count) break; /* more args than declared
            params -- shouldn't happen for a resolved call, but fail
            closed (stop) rather than read out of bounds, same
            discipline finalize_call's own identical loop already
            follows */
        AstNode *param = ctor->list.items[param_idx];
        if (param->type != NULL && param->type->kind == AST_REFERENCE_TYPE) {
            AstNode *arg_static_type = infer_expr_type(args->items[i], class_decl, locals);
            args->items[i] = address_of_if_needed(args->items[i], class_decl, locals);
            args->items[i] = cast_ref_arg_if_needed(args->items[i], arg_static_type, param->type);
        } else if (param->type != NULL && param->type->kind == AST_POINTER_TYPE) {
            /* Same plain-pointer-parameter gap finalize_call's own
             * identical loop closes -- see cast_ref_arg_if_needed's own
             * doc comment. A constructor's own plain pointer parameter
             * (`Node(Node *parent)`) called with a derived-class pointer
             * argument needs the identical cast. */
            AstNode *arg_static_type = infer_expr_type(args->items[i], class_decl, locals);
            args->items[i] = cast_ref_arg_if_needed(args->items[i], arg_static_type, param->type);
        }
    }
}

/* A best-effort, shallow-recursive clone of a default-parameter-value
 * expression (AST_PARAM's own `a` slot) -- needed because the SAME
 * default-value node would otherwise end up aliased into every call
 * site that omits that argument (`fill_default_args`, just below,
 * would append the identical `AstNode *` into two different calls'
 * own `list`s), and while that's harmless for a pass that only ever
 * READS an expression (codegen, most of this file's own recursive
 * walks), a handful of phases mutate a node's own fields in place
 * (ternary-hoisting being the clearest example -- see
 * hoist_ternaries_in_expr's own doc comment on why this file already
 * keeps several independently-reasoned-about locals lists rather than
 * share one) -- a default value visited via two different call sites
 * could then be double-transformed. Covers the expression shapes a
 * default parameter value is overwhelmingly likely to actually be in
 * practice (a literal, an identifier, a unary/binary operator
 * expression, a member access, a qualified name) -- deliberately NOT
 * exhaustive over every AST_* expression kind this project has (a
 * default value that's itself a ternary, a `new`, or a lambda-shaped
 * anything is vanishingly rare and this project doesn't even parse
 * some of those at all); an unhandled kind falls through to reusing
 * the SAME node unchanged, a documented, honest aliasing risk rather
 * than a crash, matching this project's own "miss a case rather than
 * guess wrong" philosophy everywhere else. */
static AstNode *clone_default_expr(const AstNode *n) {
    if (n == NULL) return NULL;
    switch (n->kind) {
        case AST_INT_LIT:
        case AST_FLOAT_LIT:
        case AST_CHAR_LIT:
        case AST_BOOL_LIT:
        case AST_NULL_LIT:
        case AST_IDENT:
        case AST_THIS: {
            AstNode *c = ast_new(n->kind, n->line);
            c->str1 = (n->str1 != NULL) ? strdup(n->str1) : NULL;
            c->ival = n->ival;
            c->fval = n->fval;
            return c;
        }
        case AST_STRING_LIT: {
            AstNode *c = ast_new(n->kind, n->line);
            c->str1 = (n->str1 != NULL) ? strdup(n->str1) : NULL;
            return c;
        }
        case AST_UNOP:
        case AST_CAST:
        case AST_SIZEOF: {
            AstNode *c = ast_new(n->kind, n->line);
            c->str1 = (n->str1 != NULL) ? strdup(n->str1) : NULL;
            c->type = n->type; /* a type node, not a value -- shared by
                reference everywhere else in this file too (see, e.g.,
                AST_NEW's own `type` field), never mutated in place */
            c->a = clone_default_expr(n->a);
            return c;
        }
        case AST_BINOP: {
            AstNode *c = ast_new(n->kind, n->line);
            c->str1 = (n->str1 != NULL) ? strdup(n->str1) : NULL;
            c->a = clone_default_expr(n->a);
            c->b = clone_default_expr(n->b);
            return c;
        }
        case AST_MEMBER: {
            AstNode *c = ast_new(n->kind, n->line);
            c->str1 = (n->str1 != NULL) ? strdup(n->str1) : NULL;
            c->str2 = (n->str2 != NULL) ? strdup(n->str2) : NULL;
            c->a = clone_default_expr(n->a);
            return c;
        }
        case AST_QUALIFIED_ID: {
            AstNode *c = ast_new(n->kind, n->line);
            for (int i = 0; i < n->list.count; i++) {
                ast_list_append(&c->list, clone_default_expr(n->list.items[i]));
            }
            return c;
        }
        default:
            /* See this function's own doc comment -- reused unchanged,
             * a documented aliasing risk, not a crash. */
            return (AstNode *)n;
    }
}

/* Splices default-value expressions (cloned via clone_default_expr) in
 * for every trailing parameter `target` declares beyond what
 * `call_args` actually supplies -- the C-side counterpart to real
 * C++'s own default-argument mechanism, which Vircon32 C (and even
 * plain C) has no equivalent for at all: every call this project
 * generates must supply every argument explicitly, so the ones a
 * user's call site omitted have to be filled in somewhere, and this is
 * that somewhere. `param_offset` is the same "does target->list start
 * with an injected `this`" skip finalize_call's own reference-argument
 * fixup already computes just below -- passed in rather than
 * recomputed so the two stay trivially in sync.
 *
 * A resolved call is guaranteed (by sema.c's own widened arity check,
 * resolve_overload_generic's own doc comment) to have called with at
 * least `min_required_args(target)` arguments -- every parameter this
 * loop reaches is therefore guaranteed to have a non-NULL default
 * value (`param->a`) UNLESS the call somehow reached here unresolved
 * against that guarantee; `param->a == NULL` is treated as "stop,
 * nothing more to fill" rather than a crash, matching this file's own
 * "fail closed" discipline elsewhere (see fixup_ctor_reference_args's
 * own identical stance on an out-of-range index). */
static void fill_default_args(AstList *call_args, AstNode *target, int param_offset) {
    int total_params = target->list.count - param_offset;
    for (int i = call_args->count; i < total_params; i++) {
        AstNode *param = target->list.items[i + param_offset];
        if (param->a == NULL) break;
        ast_list_append(call_args, clone_default_expr(param->a));
    }
}

/* ---- phase 3c: indirect-call receiver/argument temp hoisting ------------- */

/* File-scope temp counter, reset per function (see PLACE 3). Unique within
 * one function is all C block scope requires. */
static int g_v32_temp_counter;

/* True if this expression contains an AST_CALL anywhere. Runs AFTER phases
 * 3+4 have finalized a statement, so operator-overload uses (`list[i]`,
 * `a + b` on class operands) are already AST_CALLs and are seen through. */
static int expr_contains_call(const AstNode *n)
{
    if (n == NULL) return 0;
    if (n->kind == AST_CALL) return 1;
    if (expr_contains_call(n->a)) return 1;
    for (int i = 0; i < n->list.count; i++)
        if (expr_contains_call(n->list.items[i])) return 1;
    return 0;
}

/* If `call` is a fully-finalized VIRTUAL dispatch -- callee shaped
 * recv->vtable->Field -- return the receiver node; else NULL. Only
 * finalize_call's virtual branch ever builds a member chain whose middle
 * link is named "vtable", so the name is an unambiguous marker of an
 * indirect call site. codegen.c's virtual-destructor dispatch inside
 * v32_delete_ClassName is emitted directly as text and never appears in a
 * lowered AST, so it cannot be confused with this. */
static AstNode *virtual_call_receiver(AstNode *call)
{
    if (call == NULL || call->kind != AST_CALL || call->a == NULL) return NULL;
    AstNode *slot_ref = call->a;
    if (slot_ref->kind != AST_MEMBER || slot_ref->a == NULL) return NULL;
    AstNode *vtable_ref = slot_ref->a;
    if (vtable_ref->kind != AST_MEMBER) return NULL;
    if (vtable_ref->str2 == NULL || strcmp(vtable_ref->str2, "vtable") != 0)
        return NULL;
    return vtable_ref->a;
}

/* A receiver is "simple" (safe to emit textually twice) iff it contains no
 * call and is an identifier possibly under -> member accesses. A subscript
 * on a raw pointer array (mGrid[r][c]) contains no call and needs no
 * special case. Everything else is conservatively hoisted. */
static int receiver_is_simple(const AstNode *n)
{
    if (n == NULL) return 0;
    if (expr_contains_call(n)) return 0;
    switch (n->kind) {
        case AST_IDENT:
            return 1;
        case AST_MEMBER:
            return receiver_is_simple(n->a);
        default:
            return 0;
    }
}

/* Replace every node in the tree rooted at `n` that IS `target` (by pointer
 * identity) with a fresh identifier named `name`. Identity, not shape: the
 * receiver node is shared between the vtable-ref and (possibly cast-wrapped
 * by cast_receiver_if_needed) the first argument, and replacement must hit
 * both. */
static void replace_node_identity(AstNode *n, AstNode *target, const char *name, int line)
{
    if (n == NULL) return;
    if (n->a == target) n->a = ast_ident(name, line);
    else replace_node_identity(n->a, target, name, line);
    for (int i = 0; i < n->list.count; i++) {
        if (n->list.items[i] == target) n->list.items[i] = ast_ident(name, line);
        else replace_node_identity(n->list.items[i], target, name, line);
    }
}

/* Builds `T* v32_temp_N = <expr>;` as an AST_VAR_DECL. The type node is a
 * COPY of the expression's own inferred (already pointer-typed) type, so
 * the declaration doesn't alias the expression's type node. Mirror however
 * existing code builds local var decls (codegen prints AST_VAR_DECL via
 * print_var_decl_inline). NOTE: takes ownership of `expr`. */
static AstNode *build_hoisted_temp_decl(AstNode *expr, const char *name,
                                        AstNode *class_decl, LocalVarType *locals)
{
    AstNode *decl = ast_new(AST_VAR_DECL, expr->line);
    /* Type nodes are shared by reference throughout the codebase and never
     * mutated in place (see clone_default_expr's own comment in ast.c) --
     * no copy needed, unlike what the original patch draft assumed. */
    decl->type = infer_expr_type(expr, class_decl, locals);
    decl->str1 = strdup(name);
    decl->a = expr;
    return decl;
}

/* True if evaluating `n` unconditionally could execute something the
 * original program might not have: a ternary branch, or the RHS of a
 * short-circuit && / ||. Such expressions must NOT be hoisted. */
static int expr_has_cond_eval(const AstNode *n)
{
    if (n == NULL) return 0;
    if (n->kind == AST_TERNARY) return 1;
    if (n->kind == AST_BINOP && n->str1 != NULL &&
        (strcmp(n->str1, "&&") == 0 || strcmp(n->str1, "||") == 0))
        return 1;
    if (expr_has_cond_eval(n->a)) return 1;
    if (expr_has_cond_eval(n->b)) return 1;
    if (expr_has_cond_eval(n->c)) return 1;
    for (int i = 0; i < n->list.count; i++)
        if (expr_has_cond_eval(n->list.items[i])) return 1;
    return 0;
}

/* Insert `node` into `list` at `index`, shifting later items up. The
 * AST_LIST API only offers append (ast_list_append), so phase 3c needs
 * this small local helper to place hoisted temp decls BEFORE the
 * statement that owns them. Valid for 0 <= index <= count. */
static void ast_list_insert_at(AstList *list, int index, AstNode *node)
{
    if (index < 0 || index > list->count) return;  /* defensive; shouldn't happen */
    ast_list_append(list, node);                   /* grow (reallocs if needed) */
    for (int i = list->count - 1; i > index; i--)
        list->items[i] = list->items[i - 1];
    list->items[index] = node;
}

/* Recursive hazard scan over ONE expression tree (already finalized).
 * Hoists complex virtual-call receivers and call-containing arguments
 * into temps prepended into `block_list` at base_index + *inserted. */
static void hoist_hazards_in_expr(AstNode *expr, AstNode *stmt, AstList *block_list,
                                  int base_index, int *inserted,
                                  AstNode *class_decl, LocalVarType *locals)
{
    if (expr == NULL) return;
    /* never hoist out of a conditional-evaluation context */
    if (expr->kind == AST_TERNARY) return;
    if (expr->kind == AST_BINOP && expr->str1 != NULL &&
        (strcmp(expr->str1, "&&") == 0 || strcmp(expr->str1, "||") == 0))
        return;

    if (expr->kind == AST_CALL) {
        AstNode *recv = virtual_call_receiver(expr);
        if (recv != NULL) {
            if (!receiver_is_simple(recv)) {
                if (expr_has_cond_eval(recv)) {
                    lower_note(expr->line, "virtual-call receiver under a ternary/short-circuit: not auto-hoisted (Vircon32 C compiler workaround); hoist manually if this HALTs");
                } else {
                    char name[32];
                    snprintf(name, sizeof name, "v32_temp_%d", g_v32_temp_counter++);
                    AstNode *decl = build_hoisted_temp_decl(recv, name, class_decl, locals);
                    replace_node_identity(stmt, recv, name, expr->line);
                    ast_list_insert_at(block_list, base_index + *inserted, decl);
                    (*inserted)++;
                }
            }
            for (int i = 0; i < expr->list.count; i++) {
                AstNode *arg = expr->list.items[i];
                if (expr_contains_call(arg)) {
                    if (expr_has_cond_eval(arg)) {
                        lower_note(expr->line, "call-containing argument under a ternary/short-circuit: not auto-hoisted (Vircon32 C compiler workaround); hoist manually if this HALTs");
                    } else {
                        char name[32];
                        snprintf(name, sizeof name, "v32_temp_%d", g_v32_temp_counter++);
                        AstNode *decl = build_hoisted_temp_decl(arg, name, class_decl, locals);
                        replace_node_identity(stmt, arg, name, expr->line);
                        ast_list_insert_at(block_list, base_index + *inserted, decl);
                        (*inserted)++;
                    }
                }
            }
        }
        /* descend into every arg of ANY call -- a direct call's arguments
         * can themselves contain virtual calls needing the same treatment */
        for (int i = 0; i < expr->list.count; i++)
            hoist_hazards_in_expr(expr->list.items[i], stmt, block_list,
                                  base_index, inserted, class_decl, locals);
        return;
    }

    hoist_hazards_in_expr(expr->a, stmt, block_list, base_index, inserted, class_decl, locals);
    hoist_hazards_in_expr(expr->b, stmt, block_list, base_index, inserted, class_decl, locals);
    for (int i = 0; i < expr->list.count; i++)
        hoist_hazards_in_expr(expr->list.items[i], stmt, block_list,
                              base_index, inserted, class_decl, locals);
}

/* One statement's worth of hoisting. Only statements whose expressions
 * are evaluated UNCONDITIONALLY qualify: an expr-stmt's expression, a
 * return's expression, a var-decl's initializer, and an if's CONDITION
 * (its branches are conditional statements -- skipped; block-shaped
 * branches reach the AST_BLOCK hook through their own recursion). */
static int hoist_indirect_call_temps_stmt(AstNode *stmt, AstList *block_list,
                                          int stmt_index, AstNode *class_decl,
                                          LocalVarType *locals)
{
    int inserted = 0;
    switch (stmt->kind) {
        case AST_EXPR_STMT:
        case AST_RETURN:
        case AST_VAR_DECL:
            hoist_hazards_in_expr(stmt->a, stmt, block_list, stmt_index,
                                  &inserted, class_decl, locals);
            break;
        case AST_IF:
            hoist_hazards_in_expr(stmt->a, stmt, block_list, stmt_index,
                                  &inserted, class_decl, locals);
            break;
        default:
            break;   /* conservative: single-statement if/else branches and
                        anything else unusual is left alone */
    }
    return inserted;
}

/* True if a while/for statement's condition (or a for's step expression)
 * contains an indirect-call hazard -- used only to decide whether to warn. */
static int loop_stmt_has_indirect_hazard(AstNode *stmt)
{
    if (stmt == NULL) return 0;
    AstNode *parts[2] = { NULL, NULL };
    int n = 0;
    if (stmt->kind == AST_WHILE) {
        parts[n++] = stmt->a;                 /* condition */
    } else if (stmt->kind == AST_FOR) {
        parts[n++] = stmt->b;                 /* condition */
        parts[n++] = stmt->c;                 /* step expression */
    }
    for (int p = 0; p < n; p++) {
        AstNode *e = parts[p];
        if (e == NULL) continue;
        if (e->kind == AST_CALL) {
            AstNode *recv = virtual_call_receiver(e);
            if (recv != NULL) {
                if (!receiver_is_simple(recv)) return 1;
                for (int i = 0; i < e->list.count; i++)
                    if (expr_contains_call(e->list.items[i])) return 1;
            }
        }
        if (loop_stmt_has_indirect_hazard(e->a) ||
            loop_stmt_has_indirect_hazard(e->b)) return 1;  /* crude but
            adequate: only needs to over-warn, never under-warn... but see
            note -- replace with a proper expr walker if it misses cases */
        for (int i = 0; i < e->list.count; i++)
            if (expr_contains_call(e->list.items[i]) &&
                (e->kind == AST_CALL) == 0) return 1;  /* call somewhere in
                a non-call position is at least worth warning about */
    }
    return 0;
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
        /* Fill in any trailing default-value arguments the call site
         * itself omitted BEFORE the reference-argument fixup loop just
         * below runs -- deliberately in this order, not the reverse:
         * a freshly-spliced-in default-value expression needs that
         * same &-insertion treatment if the parameter it fills happens
         * to be a reference parameter too (`void f(Shape &s = x)` is
         * unusual but not disallowed), and running this first means
         * the loop below sees it as just another argument at its own
         * index, with no separate case needed for "was it originally
         * supplied or just filled in". */
        fill_default_args(&call->list, target, param_offset);
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
                AstNode *arg_static_type = infer_expr_type(call->list.items[i], class_decl, locals);
                call->list.items[i] = address_of_if_needed(call->list.items[i], class_decl, locals);
                call->list.items[i] = cast_ref_arg_if_needed(call->list.items[i], arg_static_type, param->type);
            } else if (param->type != NULL && param->type->kind == AST_POINTER_TYPE) {
                /* A plain (non-reference) pointer parameter -- see
                 * cast_ref_arg_if_needed's own doc comment for why this
                 * needs the identical cast a reference parameter does,
                 * for a completely different reason (sema.c never even
                 * checked this argument's type against the parameter at
                 * all, for a single-candidate call). The argument is
                 * already pointer-valued as written (`&cat`, an existing
                 * `Animal *` variable, ...) -- no address_of_if_needed
                 * step first, unlike the reference-parameter case just
                 * above. */
                AstNode *arg_static_type = infer_expr_type(call->list.items[i], class_decl, locals);
                call->list.items[i] = cast_ref_arg_if_needed(call->list.items[i], arg_static_type, param->type);
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

        /* See cast_receiver_if_needed's own doc comment above for the
         * real-Vircon32-compiler bug this closes: forwarding a const
         * object as a non-const method's receiver. Checked against
         * `obj_expr`'s OWN declared type (before address_of_if_needed
         * above may have wrapped it in `&`, which doesn't change the
         * pointee's own constness either way) and `target`'s own
         * injected `this` parameter (index 0 -- always present for a
         * resolved method call, this-injection already having run). */
        AstNode *obj_static_type = infer_expr_type(obj_expr, class_decl, locals);
        int target_this_const = (target->list.count > 0)
            ? receiver_type_is_const(target->list.items[0]->type) : 0;
        int needs_const_strip = receiver_type_is_const(obj_static_type) && !target_this_const;

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
            prepend_arg(&call->list, cast_receiver_if_needed(receiver, obj_class, canonical_class, needs_const_strip));
        } else {
            /* Non-virtual: direct call to the mangled function. */
            const AstNode *target_class = (obj_class != NULL) ? find_declaring_class(obj_class, target) : NULL;
            AstNode *arg = cast_receiver_if_needed(receiver, obj_class, target_class, needs_const_strip);
            call->a = ast_ident(target_mangled, call->line);
            prepend_arg(&call->list, arg);
        }
    } else if (callee->kind == AST_IDENT || callee->kind == AST_QUALIFIED_ID) {
        /* A free-function call, qualified or not -- finalize the
         * callee to its mangled name; there's no receiver to thread
         * through. AST_QUALIFIED_ID arrives here via resolve_call's
         * qualified branch (Fix 1); the qualifier prefix is dropped
         * wholesale because mangling is already namespace-blind. */
        call->a = ast_ident(target_mangled, call->line);
    } else {
        /* A call sema RESOLVED, whose callee shape we don't produce.
         * Emitting it unlowered silently means an unresolved symbol
         * downstream -- the exact path that let qualified calls slip
         * through unnoticed. Use the file's existing diagnostic
         * helper (same one rewrite_ternary_block reports through). */
        lower_note(call->line,
            "internal: resolved call to '%s' has unhandled callee shape %d",
            target->str1, (int)callee->kind);
    }
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

/* is_bare_free_function_ref currently begins by checking the
 * initializer's shape -- a bare AST_IDENT whose name is not a local
 * and names exactly one registered free function. Widen its entry
 * to also accept an AST_QUALIFIED_ID (e.g. `v32::draw`): take the
 * FINAL segment's name and apply the identical not-a-local,
 * exactly-one-candidate test, same flat-registry "last component
 * wins" precedent as finalize_calls_expr's own AST_QUALIFIED_ID
 * case (Fix 2-optional) and resolve_call's qualified branch.
 *
 * Everything downstream then composes unchanged: the AST_VAR_DECL /
 * AST_ASSIGN wrap sites call this BEFORE recursing, so the user's
 * original QualifiedId gets the implicit `&` wrapped around it
 * exactly once; the AST_UNOP case recurses into the operand; and
 * finalize_calls_expr's AST_QUALIFIED_ID case rewrites the operand
 * to the mangled name -- producing `(&draw__int_int_int)`, exactly
 * the shape the bare and address-of spellings already produce. */
/*
 * Real Vircon32 C, unlike standard C, does NOT implicitly decay a bare
 * function name to a function-pointer VALUE -- confirmed against the
 * actual Vircon32 C compiler (not just gcc), which rejects
 * `Callback cb = doubleIt__int;` with "types are not compatible: cannot
 * assign int(int) to int(int)*": it treats a bare function name as
 * having plain function type `int(int)`, not pointer type `int(int)*`,
 * and requires an explicit `&` to form the pointer value. Standard C
 * treats a bare function name and `&functionName` as EXACTLY the same
 * pointer value in this position (the implicit decay and the explicit
 * address-of produce an identical result), so always emitting the
 * explicit `&` form is correct and portable across BOTH of this
 * project's target dialects -- there's no need to special-case this on
 * g_target.
 *
 * These two helpers below detect exactly the narrow case that needs the
 * `&` inserted: a VarDecl initializer or a plain `=` assignment whose
 * TARGET is (possibly through a typedef chain) a function-pointer type,
 * and whose SOURCE expression, as the user actually wrote it, is a bare
 * identifier naming a free function. Anything else -- copying one
 * function-pointer-typed variable into another (`Callback cb2 = cb;`),
 * an already-explicit `&doubleIt`, a call result, ... -- is left alone.
 * Checked and wrapped in `&` BEFORE finalize_calls_expr ever recurses
 * into the identifier, so the existing AST_IDENT case below (which
 * mangles a bare free-function name to its real symbol) runs exactly
 * once, inside the new AST_UNOP wrapper, with no risk of a bare name
 * getting wrapped twice or a variable reference getting wrapped at all.
 */
static int is_bare_free_function_ref(const AstNode *expr, LocalVarType *locals) {
    const char *name;
    if (expr == NULL) return 0;
    if (expr->kind == AST_IDENT) {
        name = expr->str1;
    } else if (expr->kind == AST_QUALIFIED_ID) {
        if (expr->list.count == 0) return 0;
        name = expr->list.items[expr->list.count - 1]->str1;
    } else {
        return 0;
    }
    if (name == NULL) return 0; /* belt-and-braces: str1 can be NULL even for IDENT */
    if (find_local(locals, name) != NULL) return 0; /* a local/param
        of this name always wins, matching the AST_IDENT case below */
    AstNode **candidates = NULL;
    int count = 0, cap = 0;
    collect_free_function_candidates(name, &candidates, &count, &cap);
    free(candidates);
    return count == 1; /* same "unambiguous or don't touch it" rule as the
        AST_IDENT case below */
}

static int type_is_func_ptr(const AstNode *type) {
    if (type == NULL) return 0;
    return resolve_typedef_chain(type)->kind == AST_FUNC_PTR_TYPE;
}

/* True only for a literal integer 0 -- the one int that is a valid null
 * pointer constant in standard C, and the one Vircon32 C rejects in any
 * pointer context outright ("cannot assign int to struct T*" /
 * "types are not compatible" / "invalid operands for equality
 * comparison"), per the same real-compiler evidence AST_NULL_LIT's own
 * print_expr case in codegen.c already documents. */
static int is_int_zero_literal(const AstNode *e) {
    return e != NULL && e->kind == AST_INT_LIT && e->ival == 0;
}

/* True only for a plain pointer type (through any typedef chain).
 * Deliberately NOT AST_ARRAY_TYPE or AST_FUNC_PTR_TYPE: those have their
 * own existing lowering paths, and a 0 initializer there means something
 * else (or is already handled by type_is_func_ptr's &-insertion). */
static int type_is_plain_pointer(const AstNode *type) {
    if (type == NULL) return 0;
    return resolve_typedef_chain(type)->kind == AST_POINTER_TYPE;
}

/* Rewrites a literal `0` into an AST_NULL_LIT when the TARGET position
 * it's flowing into is a plain pointer type -- the same quirk-driven,
 * best-effort rewrite pattern as wrap_addr_of just above: Vircon32's
 * compiler rejects what standard C freely allows, so the transpiler
 * silently writes the C the user meant. `target_type` may be NULL
 * (unknown) -- then this is a no-op, matching this file's "never insert
 * a fix on a guess" rule everywhere else. */
static void rewrite_zero_to_null(AstNode **rhs_slot, const AstNode *target_type) {
    if (rhs_slot == NULL || *rhs_slot == NULL) return;
    if (!is_int_zero_literal(*rhs_slot)) return;
    if (!type_is_plain_pointer(target_type)) return;
    lower_note((*rhs_slot)->line,
        "rewrote a literal 0 to NULL in a pointer context -- Vircon32 "
        "rejects a bare int 0 assigned to or compared with a pointer, "
        "unlike standard C where 0 is a valid null pointer constant");
    AstNode *null_lit = ast_new(AST_NULL_LIT, (*rhs_slot)->line);
    *rhs_slot = null_lit;
}

/* Wraps `*slot` in an explicit `&expr` UnOp, IN PLACE -- same AST_UNOP
 * shape ("addr") that a user-written `&x` already parses to, so nothing
 * downstream (codegen's print_unop, or a later lowering pass) needs to
 * know this address-of was inserted rather than written by the user. */
static void wrap_addr_of(AstNode **slot) {
    AstNode *inner = *slot;
    lower_note(inner->line, "inserted implicit &%s -- Vircon32 doesn't decay "
        "a bare function name to a function pointer the way standard C does",
        (inner->kind == AST_IDENT && inner->str1 != NULL) ? inner->str1 : "<function>");
    AstNode *addr = ast_new(AST_UNOP, inner->line);
    addr->str1 = strdup("addr");
    addr->a = inner;
    *slot = addr;
}

static void finalize_calls_expr(AstNode **slot, AstNode *class_decl, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_IDENT: {
            /* A bare identifier used as a plain VALUE -- not a call's own
             * callee (finalize_call, below, resolves that separately via
             * the call's own CallResolution, unconditionally overwriting
             * call->a with the mangled name regardless of what ran here
             * first, so no ordering conflict with this case) and not a
             * local/parameter name (find_local, checked first, always
             * wins -- matching real C++ scoping, where a local hides a
             * same-named free function). This is the "function used as a
             * VALUE" case a function pointer initialization/assignment
             * needs (`Callback cb = doubleIt;`, or `&doubleIt`) -- a
             * real, previously-undiscovered gap, found only by actually
             * compiling (not just transpiling) tests/sample64.cpp's own
             * function-pointer test, already in this project's suite and
             * "passing" in the sense of transpiling without error: every
             * function gets a mangled name regardless of whether it's
             * overloaded (mangle()'s own doc comment, sema.c), but
             * nothing was ever rewriting a bare function-NAME reference
             * the way a call's own callee already is -- `fp = add;`
             * transpiled to the literal, unmangled `(fp = add);`, which
             * doesn't compile (confirmed directly with gcc against
             * --target=standard output: "'add' undeclared" -- only
             * `add__int_int` actually exists in the generated C).
             *
             * Deliberately conservative, matching this project's own
             * established "miss a case rather than guess wrong"
             * direction: only rewritten when EXACTLY ONE free function
             * is registered under this name
             * (collect_free_function_candidates, the same primitive
             * sema.c's own call resolution already uses for this exact
             * kind of lookup) -- an overloaded function used as a bare
             * value has no argument list here to disambiguate against,
             * and real C++'s own rule for that (matching against the
             * function pointer's own declared type) isn't modeled
             * anywhere in this project, so an ambiguous case is left
             * completely untouched rather than guessing which overload
             * was meant. A name that isn't a free function at all
             * (a global variable, an enumerator, a class name reached
             * some other way) simply gets zero candidates back and is
             * left exactly as it was -- this case is a strict no-op for
             * every identifier that isn't unambiguously a free
             * function's own name. */
            if (find_local(locals, n->str1) != NULL) break;
            AstNode **candidates = NULL;
            int count = 0, cap = 0;
            collect_free_function_candidates(n->str1, &candidates, &count, &cap);
            if (count == 1) {
                FuncSemaInfo *info = (FuncSemaInfo *)candidates[0]->sema_info;
                const char *mangled = (info != NULL) ? info->mangled_name : candidates[0]->str1;
                *slot = ast_ident(mangled, n->line);
            }
            free(candidates);
            break;
        }
        case AST_QUALIFIED_ID: {
            /* The same "function used as a VALUE" rewrite, for a
             * namespace-qualified reference (`Callback cb = v32::doubleIt;`
             * or `&v32::doubleIt`). Same reasoning as the AST_IDENT case
             * above, with the same registry precedent as Fix 1: the
             * free-function registry is flat and namespace-blind, and
             * mangling keys off the bare final name, so candidates are
             * collected by the FINAL segment only. A local cannot hide
             * here the way it can for a bare identifier -- the final
             * segment is checked against locals anyway, so a
             * same-named local still wins, matching the scoping rule
             * real C++ itself applies to the unqualified form. An
             * ambiguous (>1) or missing (0) candidate is left completely
             * untouched -- no argument list exists here to disambiguate
             * against, and this project deliberately doesn't model
             * matching against the function pointer's declared type. */
            if (n->list.count == 0) break;
            const char *final = n->list.items[n->list.count - 1]->str1;
            if (find_local(locals, final) != NULL) break;
            AstNode **candidates = NULL;
            int count = 0, cap = 0;
            collect_free_function_candidates(final, &candidates, &count, &cap);
            if (count == 1) {
                FuncSemaInfo *info = (FuncSemaInfo *)candidates[0]->sema_info;
                const char *mangled = (info != NULL) ? info->mangled_name
                                                     : candidates[0]->str1;
                *slot = ast_ident(mangled, n->line);
            }
            free(candidates);
            break;
        }
        case AST_MEMBER:
            finalize_calls_expr(&n->a, class_decl, locals);
            break;
        case AST_CALL: {
            finalize_calls_expr(&n->a, class_decl, locals);
            for (int i = 0; i < n->list.count; i++) {
                finalize_calls_expr(&n->list.items[i], class_decl, locals);
            }
            /* Deliberately NOT applying strip_const_member_read to call
             * arguments here, unlike AST_ASSIGN's RHS and AST_VAR_DECL's
             * initializer just below: finalize_call (called right after
             * this loop) still needs to inspect a REFERENCE argument's
             * own bare expression to decide whether to wrap it in `&`
             * (address_of_if_needed) -- wrapping it in a cast first
             * would turn it into an rvalue, and taking the address of a
             * cast expression isn't valid C. A by-VALUE argument reading
             * a const member (not a reference one) could in principle
             * hit the same "discards const qualifier" error this fixes
             * elsewhere, but no test in this project has actually
             * surfaced that case against a real Vircon32 compiler run
             * yet -- left as a known, honestly-stated gap rather than
             * guessed at, matching this project's own established
             * practice (see this file's many other "found only by an
             * actual compiler run" comments). */
            finalize_call(n, class_decl, locals);
            /* NEW: closes the "known, honestly-stated gap" the comment
             * above used to carry. Now that finalize_call has finished
             * every &-insertion and receiver/argument cast it's going to
             * do, any argument that is STILL a bare AST_MEMBER is a
             * by-value member read -- and if its receiver is const
             * (`this->mFrame` inside a const method), Vircon32 rejects
             * passing it to a plain int parameter ("cannot assign const
             * int to int: discards const qualifier"). Reference arguments
             * are never touched: they're already wrapped in an AST_UNOP
             * "addr" or an AST_CAST by now, so strip_const_member_read's
             * own bare-AST_MEMBER-only guard skips them for free --
             * closing the gap without reintroducing the rvalue problem
             * the original ordering restriction existed to avoid.
             * Surfaced by a real program: a const Alien::spriteId()
             * dispatching this->mFrame through the vtable. */
            for (int i = 0; i < n->list.count; i++) {
                n->list.items[i] = strip_const_member_read(n->list.items[i], class_decl, locals);
            }
            /* Reference-RETURN calls need a deref inserted at their use
             * site -- symmetric to reference-PARAMETER arguments needing
             * an addr-of inserted at the call site (finalize_call's own
             * comment on that, above). A C++ reference return acts like
             * the referent itself; once lowered to a real pointer
             * return, an ordinary use of the call's result needs an
             * explicit dereference to keep meaning what it meant in
             * C++ -- otherwise `int x = obj.getRef();` would assign the
             * ADDRESS instead of the value. Not a guess: caught against
             * an actual build of tests/sample72.cpp, where
             * `int viaReference = original->getValueRef();` generated
             * `int viaReference = Box__getValueRef__void(original);`,
             * assigning an `int *` into an `int` -- exactly the kind of
             * mismatch Vircon32's own C compiler already rejects
             * elsewhere in this project (see address_of_if_needed's own
             * doc comment for a matching real example).
             *
             * Checked against `target`'s type BEFORE phase 5 relabels
             * any AST_REFERENCE_TYPE to AST_POINTER_TYPE, same ordering
             * requirement as inject_reference_return_address_stmt above
             * (see its own comment for why this phase has to run before
             * phase 5, not after).
             *
             * SCOPE LIMITATION: this always inserts the deref, with no
             * check for whether the call's result is itself flowing into
             * a reference-typed local (`int &r = obj.getRef();`, which
             * would want to keep the pointer, not dereference it) --
             * reference-typed LOCALS are their own separate, already-
             * incomplete area of this project (their own initializers
             * don't get address-of treatment either; see
             * fix_reference_access_stmt's AST_VAR_DECL case, which only
             * fixes up `.`/`->` access, never initialization), and no
             * test anywhere in this project currently combines the two.
             * Left as a known, honestly-stated limitation rather than
             * guessed at, matching this project's established practice
             * everywhere else in this file. */
            CallResolution *cr = (CallResolution *)n->sema_info;
            if (cr != NULL && cr->resolved_target != NULL) {
                AstNode *target = cr->resolved_target;
                if (target->type != NULL && target->type->kind == AST_REFERENCE_TYPE) {
                    AstNode *deref = ast_new(AST_UNOP, n->line);
                    deref->str1 = strdup("deref"); /* same AST_UNOP shape
                        used everywhere else in this project for `*expr`
                        -- print_unop (codegen.c) already knows "deref"
                        means "(*expr)"; no new AST kind needed */
                    deref->a = n;
                    *slot = deref;
                }
            }
            break;
        }
        case AST_BINOP:
            finalize_calls_expr(&n->a, class_decl, locals);
            finalize_calls_expr(&n->b, class_decl, locals);
            /* NEW: `ptr == 0` / `ptr != 0` -- the comparison counterpart
             * of the AST_ASSIGN case's null rewrite. Vircon32 rejects a
             * pointer/int-literal comparison outright ("invalid operands
             * for equality comparison"), so rewrite the literal 0 side to
             * NULL whenever the OTHER side is statically a plain pointer.
             * Only == and !=: every other binop on a pointer is already
             * invalid C++ that sema would have flagged. Done before
             * rewrite_operator_use so an operator== overload, if sema
             * resolved one, sees the same operand shapes it already
             * expects (a pointer-vs-null comparison never resolves to an
             * overload, so this is a no-op in that path). */
            if (n->str1 != NULL &&
                (strcmp(n->str1, "==") == 0 || strcmp(n->str1, "!=") == 0)) {
                AstNode *a_type = infer_expr_type(n->a, class_decl, locals);
                AstNode *b_type = infer_expr_type(n->b, class_decl, locals);
                if (type_is_plain_pointer(a_type)) {
                    rewrite_zero_to_null(&n->b, a_type);
                } else if (type_is_plain_pointer(b_type)) {
                    rewrite_zero_to_null(&n->a, b_type);
                }
            }
            rewrite_operator_use(slot, n->a, n->b, class_decl, locals);
            break;
        case AST_ASSIGN:
            finalize_calls_expr(&n->a, class_decl, locals);
            /* Plain `fp = someFunctionName;` needs the same implicit
             * `&` that a VarDecl initializer needs -- see the doc
             * comment on is_bare_free_function_ref/type_is_func_ptr
             * above. Checked (and n->a already finalized, so
             * infer_expr_type sees a real, resolvable LHS) before
             * n->b is recursed into, so the wrap happens exactly once,
             * around the user's original bare identifier. Scoped to
             * plain "=" only -- a compound assignment on a function
             * pointer isn't meaningful and isn't supported anywhere
             * else in this project either. */
            if (n->str1 != NULL && strcmp(n->str1, "=") == 0 &&
                is_bare_free_function_ref(n->b, locals) &&
                type_is_func_ptr(infer_expr_type(n->a, class_decl, locals))) {
                wrap_addr_of(&n->b);
            }
            /* NEW: `mItems[i] = 0;` / `mPlayerBullet = 0;` -- a literal 0
             * stored into a pointer-typed LHS. Checked after n->a is
             * finalized (so infer_expr_type sees a resolvable LHS) and
             * before n->b is recursed into (the literal has nothing to
             * recurse into). Scoped to plain "=" only, same as the
             * func-ptr &-insertion just above. */
            if (n->str1 != NULL && strcmp(n->str1, "=") == 0) {
                rewrite_zero_to_null(&n->b, infer_expr_type(n->a, class_decl, locals));
            }
            finalize_calls_expr(&n->b, class_decl, locals);
            /* A bare member read on the RHS through a const receiver
             * (`this->size = other->size;`, a copy constructor's own
             * canonical body) needs the same const-strip cast
             * cast_receiver_if_needed already gives method receivers --
             * see strip_const_member_read's own doc comment for the
             * real Vircon32-compiler error this closes. Applied here,
             * to the RHS only (never n->a, the assignment's own LHS --
             * see strip_const_member_read's doc comment for why an
             * lvalue position must never be wrapped), after n->b's own
             * calls are already finalized but before any operator-
             * overload rewrite below, so rewrite_operator_use always
             * sees the same shape it already expected (this is scoped
             * to `AST_MEMBER` only, so it's a no-op for every
             * operator-overload case, which never produces a bare
             * AST_MEMBER on this side). */
            n->b = strip_const_member_read(n->b, class_decl, locals);
            /* Assigning a derived-class pointer into a base-class pointer
             * variable (`Alien *a; a = new AlienTopRow(px, py);`, the
             * polymorphic-factory pattern Swarm::spawn uses to build one
             * of several Alien subclasses into a single Alien* local)
             * needs the same explicit base-class cast a reference-bound
             * ARGUMENT already gets from cast_ref_arg_if_needed (above) --
             * a real, separate gap: this project's single-inheritance
             * struct layout makes the assignment memory-safe, but C's
             * type system has no way to know that (confirmed via gcc:
             * `-Wincompatible-pointer-types`, "assignment to 'struct
             * Alien *' from incompatible pointer type 'struct
             * AlienTopRow *'"), and this project's own established
             * pattern (cast_receiver_if_needed's own doc comment) is that
             * a real Vircon32 compiler run is likely to reject outright
             * what gcc only warns about. Scoped to plain "=" only, and
             * only when BOTH sides are statically known to be POINTER
             * types (never a plain by-value object assignment -- struct
             * slicing a derived object into a base one needs an actual
             * memberwise copy this project doesn't generate, an honest,
             * separate, out-of-scope limitation, not something a pointer
             * cast could paper over safely). The RHS's class is read
             * directly off `n->b->type` when it's a bare `new` expression
             * (infer_expr_type has no AST_NEW case at all -- new_delete_
             * rewrite_expr, phase 6, hasn't run yet at this point in
             * phase 3/4, so n->b is still the original AST_NEW node
             * here), falling back to infer_expr_type for every other RHS
             * shape (an existing derived-pointer variable/expression
             * being assigned across, not just a fresh `new`). */
            if (n->str1 != NULL && strcmp(n->str1, "=") == 0) {
                AstNode *lhs_type = infer_expr_type(n->a, class_decl, locals);
                if (lhs_type != NULL && lhs_type->kind == AST_POINTER_TYPE) {
                    AstNode *lhs_class = type_to_class(lhs_type);
                    AstNode *rhs_class = NULL;
                    if (n->b->kind == AST_NEW) {
                        rhs_class = type_to_class(n->b->type);
                    } else {
                        AstNode *rhs_type = infer_expr_type(n->b, class_decl, locals);
                        if (rhs_type != NULL && rhs_type->kind == AST_POINTER_TYPE) {
                            rhs_class = type_to_class(rhs_type);
                        }
                    }
                    if (lhs_class != NULL && rhs_class != NULL && lhs_class != rhs_class) {
                        AstNode *cast = ast_new(AST_CAST, n->line);
                        cast->type = ast_wrap_pointer(ast_ident(lhs_class->str1, n->line), n->line);
                        cast->a = n->b;
                        n->b = cast;
                        lower_note(n->line, "inserted (%s *) cast for a base/"
                            "derived pointer assignment", lhs_class->str1);
                    }
                }
            }
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
            /* A reference-parameter constructor argument (a copy
             * constructor's own `other`, most commonly) needs the same
             * implicit `&` this phase's own AST_CALL case already
             * inserts for an ordinary call's reference arguments
             * (finalize_call, above) -- a real, previously-undiscovered
             * bug found alongside the copy-constructor overload-matching
             * fix in sema.c (type_matches_param): confirmed directly
             * against real gcc output ("incompatible type for
             * argument... expected 'const struct Shape *' but argument
             * is of type 'struct Shape'"). MUST run here, in THIS phase,
             * not in new_delete_rewrite_expr (phase 6) where the actual
             * allocator call gets built -- by phase 6, fix_references
             * (phase 5) has already relabeled the resolved constructor's
             * own AST_REFERENCE_TYPE parameter to AST_POINTER_TYPE
             * in-place on the shared target node (finalize_call's own
             * doc comment, above, already states and relies on this
             * exact ordering constraint for the identical reason), so
             * fixup_ctor_reference_args's own "is this parameter still a
             * reference" check would silently never fire that late --
             * confirmed the hard way, by watching this exact fix fail
             * silently when first placed in phase 6 instead. sema.c's
             * resolve_new_expr has already run (sema_run always
             * completes before lower_run starts at all), so
             * `n->sema_info` is already populated here despite this
             * being the very first lowering phase to touch expressions
             * at all. */
            {
                CallResolution *cr = (CallResolution *)n->sema_info;
                if (cr != NULL && cr->resolved_target != NULL) {
                    fixup_ctor_reference_args(&n->list, cr->resolved_target, class_decl, locals);
                }
            }
            break;
        case AST_DIRECT_INIT:
            /* Same reasoning as AST_NEW just above: only the constructor
             * arguments (`Shape shape(getSize());`) can contain a nested
             * call needing finalization -- there's no separate "type
             * being allocated" or array-size expression to also walk
             * here, unlike AST_NEW. Same reference-parameter `&`-
             * insertion fix too, same ordering requirement (must run
             * before phase 5 relabels the resolved constructor's own
             * reference parameter away) -- see the identical comment on
             * AST_NEW's own case just above for the full account. */
            for (int i = 0; i < n->list.count; i++) {
                finalize_calls_expr(&n->list.items[i], class_decl, locals);
            }
            {
                CallResolution *cr = (CallResolution *)n->sema_info;
                if (cr != NULL && cr->resolved_target != NULL) {
                    fixup_ctor_reference_args(&n->list, cr->resolved_target, class_decl, locals);
                }
            }
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
            /* Runs after each direct child is fully finalized: at this point
             * every expression has its final dispatch shape (phases 3 and 4
             * interleave inside finalize_call/rewrite_operator_use, so there is
             * no earlier moment when the whole statement is final). Only DIRECT
             * children are scanned here -- nested blocks recurse through this
             * same case and hoist into their own block, keeping loop bodies
             * re-evaluating their temps per iteration. */
            for (int i = 0; i < n->list.count; i++) {
                AstNode *stmt = n->list.items[i];
                if (stmt == NULL) continue;
                if (stmt->kind == AST_WHILE || stmt->kind == AST_FOR) {
                    /* single-statement bodies aren't Blocks (the parser only wraps
                     * multi-statement ones) -- wrap them here so their hazards get
                     * the same hoisting a braced body would. Purely structural. */
                    AstNode *body = (stmt->kind == AST_FOR) ? stmt->d : stmt->b;
                    if (body != NULL && body->kind != AST_BLOCK) {
                        AstNode *blk = ast_new(AST_BLOCK, body->line);
                        ast_list_append(&blk->list, body);
                        if (stmt->kind == AST_FOR) stmt->d = blk;
                        else                       stmt->b = blk;
                        body = blk;
                    }
                    if (body != NULL && body->kind == AST_BLOCK) {
                        for (int j = 0; j < body->list.count; j++) {
                            AstNode *inner = body->list.items[j];
                            if (inner == NULL) continue;
                            if (inner->kind == AST_WHILE || inner->kind == AST_FOR) continue; /* nesting handled by its own visit */
                            j += hoist_indirect_call_temps_stmt(inner, &body->list, j,
                                                                 class_decl, *locals);
                        }
                    }
                    /* condition (and for's init/step) still deliberately not hoisted:
                     * temps there would change evaluation frequency */
                    if (loop_stmt_has_indirect_hazard(stmt))
                        lower_note(stmt->line, "virtual call with call-result receiver/argument inside a while/for condition: not auto-hoisted (Vircon32 C compiler workaround); hoist manually if this HALTs");
                    continue;
                }
                i += hoist_indirect_call_temps_stmt(stmt, &n->list, i, class_decl, *locals);
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
            finalize_calls_expr(&n->a, class_decl, *locals);
            /* `return other.size;`, reading a const member value back
             * out of a const-reference parameter, hits the exact same
             * real-Vircon32-compiler-only error strip_const_member_read
             * closes for an assignment's RHS -- see its own doc comment.
             * Applied here too since a function's return value is
             * exactly as much a value-read position as an assignment's
             * RHS is. */
            n->a = strip_const_member_read(n->a, class_decl, *locals);
            break;
        case AST_EXPR_STMT:
            finalize_calls_expr(&n->a, class_decl, *locals);
            break;
        case AST_VAR_DECL: {
            /* `Callback cb = doubleIt;` needs the same implicit `&`
             * as the AST_ASSIGN case above -- see the doc comment on
             * is_bare_free_function_ref/type_is_func_ptr in
             * finalize_calls_expr. Checked against n->type (this
             * VarDecl's own declared type, resolved through any
             * typedef chain) BEFORE recursing into the initializer,
             * same reasoning as the AST_ASSIGN case: exactly one
             * wrap, around the user's original bare identifier. */
            if (is_bare_free_function_ref(n->a, *locals) && type_is_func_ptr(n->type)) {
                wrap_addr_of(&n->a);
            }
            finalize_calls_expr(&n->a, class_decl, *locals);
            /* `int size = other.size;`, the intro-level "copy a const
             * reference's field into a plain local" pattern -- same
             * const-strip fix as the AST_ASSIGN/AST_RETURN cases, see
             * strip_const_member_read's own doc comment. Skipped for a
             * REFERENCE-typed local (`int &r = ...`): a reference needs
             * an addressable lvalue to bind to, not a cast rvalue, and
             * this project's reference-local support is already its own
             * separate, incomplete area (see finalize_calls_expr's own
             * AST_CALL case comment on reference-return derefs, above,
             * for the established precedent of leaving that case alone
             * rather than guessing at it here too). */
            if (n->type == NULL || n->type->kind != AST_REFERENCE_TYPE) {
                n->a = strip_const_member_read(n->a, class_decl, *locals);
            }
            /* NEW: `Alien *a = 0;` -- the VarDecl-initializer counterpart
             * of the AST_ASSIGN null rewrite. n->type is this decl's own
             * declared type; a reference-typed local is skipped for free
             * (AST_REFERENCE_TYPE is not AST_POINTER_TYPE at this point
             * in the pipeline, and phase 5 relabels those later). */
            rewrite_zero_to_null(&n->a, n->type);

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

/* Wraps `return expr` in address_of_if_needed when the enclosing
 * function's own return type is AST_REFERENCE_TYPE -- a C++ reference
 * return implicitly takes the address of whatever's returned, the same
 * "needs &, unless the expression is already a pointer" transformation
 * address_of_if_needed already gives method receivers (just above) and
 * reference-typed call arguments (finalize_call's own comment on that,
 * higher in this file). VIRCON32_QUIRKS.md entry #12: closing this gap
 * is what lets a function actually return T& (or T*) at all, on top of
 * the earlier, parameter-only fix.
 *
 * MUST run before phase 5 (fix_references) ever relabels
 * AST_REFERENCE_TYPE to AST_POINTER_TYPE anywhere in the program --
 * once that's happened there is no way left to tell a true pointer
 * return from a lowered reference one, the exact same ordering hazard
 * finalize_call's own reference-parameter fix already explains for
 * parameters (see that comment for the full reasoning; it applies here
 * unchanged).
 *
 * Mirrors finalize_calls_stmt's own traversal shape exactly, including
 * its VAR_DECL locals-tracking -- but DELIBERATELY runs as a fully
 * separate pass with its own freshly-seeded locals list, not sharing
 * finalize_calls_stmt's, for the same reason fix_references_in_method's
 * own seed_locals_with_reference_tracking is kept separate from phase
 * 3's seed_locals_from_params (see that comment, above): a shared/
 * aliased locals list here would just be redundant bookkeeping, not a
 * correctness requirement, but keeping this walk independent means it
 * can be reasoned about (and tested) entirely on its own, without
 * depending on finalize_calls_stmt happening to run first in the same
 * call. */
static void inject_reference_return_address_stmt(AstNode **slot, AstNode *class_decl, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK:
            for (int i = 0; i < n->list.count; i++) {
                inject_reference_return_address_stmt(&n->list.items[i], class_decl, locals);
            }
            break;
        case AST_IF:
            inject_reference_return_address_stmt(&n->b, class_decl, locals);
            inject_reference_return_address_stmt(&n->c, class_decl, locals);
            break;
        case AST_LABEL:
            inject_reference_return_address_stmt(&n->a, class_decl, locals);
            break;
        case AST_WHILE:
            inject_reference_return_address_stmt(&n->b, class_decl, locals);
            break;
        case AST_FOR:
            inject_reference_return_address_stmt(&n->a, class_decl, locals);
            inject_reference_return_address_stmt(&n->d, class_decl, locals);
            break;
        case AST_RETURN:
            if (n->a != NULL) {
                n->a = address_of_if_needed(n->a, class_decl, *locals);
            }
            break;
        case AST_VAR_DECL: {
            LocalVarType *lv = calloc(1, sizeof(LocalVarType)); /* calloc: zero-inits was_reference too */
            lv->name = n->str1;
            lv->type = n->type;
            lv->next = *locals;
            *locals = lv;
            break;
        }
        case AST_ASM:
            /* Nothing to do: an asm body is opaque string literals --
             * no `this`, member references, or references to rewrite.
             * Passed through to codegen.c verbatim. */
            break;
        default:
            break;
    }
}

static void finalize_calls_in_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return;
    g_v32_temp_counter = 0;                        /* NEW: phase 3c */
    LocalVarType *locals = seed_locals_from_params(method);
    finalize_calls_stmt(&method->a, class_decl, &locals);
    if (method->type != NULL && method->type->kind == AST_REFERENCE_TYPE) {
        LocalVarType *ret_locals = seed_locals_from_params(method);
        inject_reference_return_address_stmt(&method->a, class_decl, &ret_locals);
    }
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
            g_v32_temp_counter = 0;                        /* NEW: phase 3c */
            finalize_calls_stmt(&n->a, NULL, &locals);
            if (n->type != NULL && n->type->kind == AST_REFERENCE_TYPE) {
                /* Same reference-return handling as
                 * finalize_calls_in_method just above -- a free function
                 * can return T& just as much as a method can. */
                LocalVarType *ret_locals = seed_locals_from_params(n);
                inject_reference_return_address_stmt(&n->a, NULL, &ret_locals);
            }
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
        case AST_DIRECT_INIT:
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
    if (method->type != NULL && method->type->kind == AST_REFERENCE_TYPE) {
        /* A function's OWN return type needs the exact same relabeling
         * seed_locals_with_reference_tracking already gives every
         * parameter/local just above (AST_REFERENCE_TYPE -> same node,
         * just AST_POINTER_TYPE) -- this was never done here before
         * VIRCON32_QUIRKS.md entry #12's fix, confirmed by grep across
         * this whole file first, not assumed. The corresponding
         * "needs &" transformation at every `return expr` site is phase
         * 3/4's job (inject_reference_return_address_stmt, above,
         * already run by finalize_calls_in_method before this phase
         * ever reaches here) -- this is only the type-label half. */
        method->type->kind = AST_POINTER_TYPE;
    }
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
            if (n->type != NULL && n->type->kind == AST_REFERENCE_TYPE) {
                /* Same reference-return relabeling as
                 * fix_references_in_method just above -- a free function
                 * can return T& just as much as a method can. */
                n->type->kind = AST_POINTER_TYPE;
            }
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
                /* Reference-parameter arguments (a copy constructor's
                 * own `other`, most commonly) already got their implicit
                 * `&` inserted earlier, in finalize_calls_expr's own
                 * AST_NEW case (phase 3/4) -- see that case's own doc
                 * comment for exactly why it has to happen THERE and not
                 * here (phase 5 has already relabeled the resolved
                 * constructor's own reference parameter to a plain
                 * pointer by the time this phase runs, so the same check
                 * repeated here would silently never fire). */
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
        case AST_DIRECT_INIT:
            /* `Shape shape(new Widget);` -- a stack local's own direct-
             * init constructor arguments can themselves contain a
             * nested `new`/`delete` needing this same rewriting, same
             * as any other argument list this phase already walks. */
            for (int i = 0; i < n->list.count; i++) {
                new_delete_rewrite_expr(&n->list.items[i], class_decl, locals);
            }
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

/* Finds `class_decl`'s own constructor that's CALLABLE with zero
 * arguments -- either it truly declares none at all (`m->list.count ==
 * 1`, just the injected "this"), or every one of its real parameters
 * has a default value (default parameter values, this project's own
 * later addition -- see AST_PARAM's own doc comment in ast.h and
 * min_required_args's own doc comment in sema.c). Real C++ calls both
 * shapes a "default constructor" for exactly this reason: `Random()`
 * and `Random(int seed = 1234)` are equally valid for `Random r;` to
 * resolve against. Has a body -- see this phase's own doc comment
 * above for why that matters. Constructors are never inherited in
 * C++, so this only ever needs to check `class_decl`'s OWN methods
 * list, unlike find_declaring_class's ancestor-walking elsewhere in
 * this file.
 *
 * A caller building a call to whatever this returns must NOT assume
 * "zero arguments" any more, now that the second shape is possible --
 * `this` still needs to be passed, and building a correct call also
 * means splicing in the constructor's own default-value expressions
 * for any real parameter beyond `this`, exactly the way
 * fill_default_args already does for an ordinary resolved call. Every
 * call site below that uses this function's result does so. */
/* Builds `objExpr.vtable = &ClassName_vtable_instance;` as a standalone
 * AST_EXPR_STMT -- the stack-object counterpart to the fix
 * emit_new_delete_runtime (codegen.c) now applies for a HEAP-allocated
 * one: a class with a vtable but NO user-declared constructor at all
 * has no constructor BODY anywhere for phase 8 (inject_vtable_init_
 * classes, above) to have injected its own vtable-pointer assignment
 * into, so a plain `Square s;` (no `new`, no explicit initializer)
 * previously left `s.vtable` as raw, uninitialized stack garbage --
 * the exact same real, previously-undiscovered bug class, just found
 * on the stack instead of the heap. Confirmed directly (not guessed
 * at) with an isolated repro that segfaulted on the very first virtual
 * call through such an object before this fix, and ran clean after.
 * Called from both of this phase's own "no constructor at all" call
 * sites below -- the plain scalar VarDecl case and the per-element
 * stack-array case -- each of which builds its own `objExpr` (a bare
 * AST_IDENT for a scalar, an AST_SUBSCRIPT for an array element) and
 * passes it in fresh, used exactly once.
 *
 * `.` access (not `->`) since `objExpr` is always a plain VALUE
 * expression here, never a pointer -- matching how this project prints
 * ordinary stack-object member access everywhere else. Returns NULL
 * when the class has no vtable at all -- nothing to initialize, and
 * the caller already knows to fall back to its own prior "nothing to
 * inject" no-op in that case, matching this file's "only act when
 * there's something real to fix" discipline everywhere else. */
static AstNode *build_vtable_init_stmt(AstNode *obj_expr, AstNode *class_decl, int line) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL || layout->vtable == NULL) return NULL;

    AstNode *vtable_ref = ast_new(AST_MEMBER, line);
    vtable_ref->str1 = strdup(".");
    vtable_ref->str2 = strdup("vtable");
    vtable_ref->a = obj_expr;

    size_t len = strlen(class_decl->str1) + strlen("_vtable_instance") + 1;
    char *instance_name = malloc(len);
    snprintf(instance_name, len, "%s_vtable_instance", class_decl->str1);
    AstNode *addr = ast_new(AST_UNOP, line);
    addr->str1 = strdup("addr");
    addr->a = ast_ident(instance_name, line);
    free(instance_name);

    AstNode *assign = ast_new(AST_ASSIGN, line);
    assign->str1 = strdup("=");
    assign->a = vtable_ref;
    assign->b = addr;

    AstNode *expr_stmt = ast_new(AST_EXPR_STMT, line);
    expr_stmt->a = assign;
    return expr_stmt;
}

static AstNode *find_zero_arg_constructor(AstNode *class_decl) {
    ClassLayout *layout = (ClassLayout *)class_decl->sema_info;
    if (layout == NULL) return NULL;
    for (int i = 0; i < layout->methods.count; i++) {
        AstNode *m = layout->methods.items[i];
        if (strcmp(m->str1, class_decl->str1) != 0) continue; /* not a constructor at all */
        if (m->kind != AST_FUNC_DEF) continue; /* no body -- see doc comment */
        if (m->list.count == 1) return m; /* just the injected "this" -- zero explicit params */
        int all_defaulted = 1;
        for (int p = 1; p < m->list.count; p++) {
            if (m->list.items[p]->a == NULL) { all_defaulted = 0; break; }
        }
        if (all_defaulted) return m;
    }
    return NULL;
}

static void inject_ctor_calls_stmt(AstNode **slot, AstNode *class_decl, LocalVarType **locals, int *arr_ctor_counter);

/* Builds the per-element constructor-call loop a stack array of class
 * objects needs -- `Shape shapes[3];` -- the array counterpart to the
 * scalar case just above (inject_ctor_calls_block's own `stmt->a ==
 * NULL` branch): once VIRCON32_QUIRKS.md's own README entry on this
 * ("the most dangerous gap on this list precisely because nothing
 * about it looks wrong until the program runs") was actually looked
 * at, closing it turned out to need no new AST node kind and no new
 * lowering phase at all -- just one more shape for this EXISTING
 * phase to build, reusing machinery (AST_FOR, AST_SUBSCRIPT, ordinary
 * `post++`) this project already has everywhere else.
 *
 * Produces:
 *   for (int __v32_ctor_arr_iN = 0; __v32_ctor_arr_iN < LEN; __v32_ctor_arr_iN++)
 *       ClassName__ClassName__void(&arrayName[__v32_ctor_arr_iN]);
 *
 * `arr_ctor_counter` gives each such loop its own uniquely-numbered
 * index variable within the enclosing function -- the same "per-
 * function counter, threaded through by pointer, incremented on use"
 * pattern this file already uses for __v32_ret_tmpN (destruct_scope_*)
 * and __v32_tern_tmpN (hoist_ternaries_in_expr); needed the moment two
 * such arrays appear in the same function, which no single-array test
 * alone would ever catch.
 *
 * Scope limitations, matching the zero-arg scalar case's own (see this
 * phase's top-of-file doc comment): only a stack array declared
 * directly as a BLOCK statement is handled, not one appearing in a
 * for-loop's own init clause (vanishingly rare for an array anyway);
 * a multi-dimensional array (`Shape grid[2][2];`, itself not otherwise
 * exercised by this project's test suite for CLASS element types) only
 * gets its OUTER dimension's elements constructed, since `LEN` here is
 * simply `stmt->type->ival` and this project's own AST_ARRAY_TYPE
 * nesting for a multi-dimensional array means the "element type" at
 * this level is itself another AST_ARRAY_TYPE, not a class -- so
 * type_to_class on it already returns NULL and this whole function is
 * never even called for that case, a "miss a case rather than guess
 * wrong" no-op matching everything else in this file, not a silent
 * half-fix. */
static AstNode *build_array_ctor_loop(AstNode *stmt, AstNode *var_class, int *arr_ctor_counter) {
    AstNode *ctor = find_zero_arg_constructor(var_class);

    char idx_name[64];
    snprintf(idx_name, sizeof(idx_name), "__v32_ctor_arr_i%d", *arr_ctor_counter);

    AstNode *subscript = ast_new(AST_SUBSCRIPT, stmt->line);
    subscript->a = ast_ident(stmt->str1, stmt->line);
    subscript->b = ast_ident(idx_name, stmt->line);

    AstNode *body_stmt;
    if (ctor != NULL) {
        FuncSemaInfo *info = (FuncSemaInfo *)ctor->sema_info;
        const char *mangled = (info != NULL) ? info->mangled_name : ctor->str1;

        AstNode *addr = ast_new(AST_UNOP, stmt->line);
        addr->str1 = strdup("addr");
        addr->a = subscript;

        /* fill_default_args (see its own doc comment) expects an argument
         * list that does NOT include the receiver -- exactly like
         * fixup_ctor_reference_args's own `args` parameter -- so it's
         * called on an EMPTY temporary list here (this call site always
         * supplies zero explicit real arguments, by construction: see
         * find_zero_arg_constructor's own doc comment on why that no
         * longer means the constructor takes none at all), and the
         * receiver is prepended into `call->list` separately, after. */
        AstList real_args = ast_list_new();
        fill_default_args(&real_args, ctor, 1); /* offset 1 -- ctor->list
            starts with the injected "this" */

        AstNode *call = ast_new(AST_CALL, stmt->line);
        call->a = ast_ident(mangled, stmt->line);
        ast_list_append(&call->list, addr);
        for (int k = 0; k < real_args.count; k++) {
            ast_list_append(&call->list, real_args.items[k]);
        }

        body_stmt = ast_new(AST_EXPR_STMT, stmt->line);
        body_stmt->a = call;
    } else {
        /* No user-declared constructor at all -- see build_vtable_init_
         * stmt's own doc comment for the real bug this closes: a
         * vtable-owning class with no constructor previously got NO
         * per-element vtable-pointer initialization either, for the
         * exact same underlying reason emit_new_delete_runtime's own
         * doc comment (codegen.c) describes for the heap-`new` case --
         * `subscript` (an array element, a plain VALUE, never a
         * pointer) is exactly the right shape build_vtable_init_stmt
         * already expects. */
        body_stmt = build_vtable_init_stmt(subscript, var_class, stmt->line);
        if (body_stmt == NULL) return NULL; /* no ctor AND no vtable --
            truly nothing to do, matching this function's own prior
            "ctor == NULL" no-op for a class that needs neither */
    }
    (*arr_ctor_counter)++;

    AstNode *idx_decl = ast_new(AST_VAR_DECL, stmt->line);
    idx_decl->str1 = strdup(idx_name);
    idx_decl->type = ast_ident("int", stmt->line);
    idx_decl->a = ast_new(AST_INT_LIT, stmt->line);
    idx_decl->a->ival = 0;

    AstNode *cond = ast_new(AST_BINOP, stmt->line);
    cond->str1 = strdup("<");
    cond->a = ast_ident(idx_name, stmt->line);
    cond->b = ast_new(AST_INT_LIT, stmt->line);
    cond->b->ival = stmt->type->ival;

    AstNode *step = ast_new(AST_UNOP, stmt->line);
    step->str1 = strdup("post++");
    step->a = ast_ident(idx_name, stmt->line);

    AstNode *body = ast_new(AST_BLOCK, stmt->line);
    ast_list_append(&body->list, body_stmt);

    AstNode *loop = ast_new(AST_FOR, stmt->line);
    loop->a = idx_decl;
    loop->b = cond;
    loop->c = step;
    loop->d = body;
    return loop;
}

/* Rebuilds `block`'s own statement list, inserting a constructor call
 * immediately after any VarDecl that needs one. Recurses into each
 * statement FIRST (so a nested block's own VarDecls get handled too)
 * before appending it -- and any inserted call -- to the new list.
 *
 * `class_decl`/`locals` were added alongside this project's own
 * direct-initialization support specifically so fixup_ctor_reference_
 * args (called from the AST_DIRECT_INIT branch below) has what it
 * needs to call infer_expr_type on a constructor argument -- this
 * phase never needed either one before, since the pre-existing zero-
 * argument case has no arguments to inspect at all. `*locals` is kept
 * current across the whole block (every VarDecl this phase sees, not
 * just the direct-init ones), the same "independently reasoned about"
 * per-phase locals list this project already keeps in several other
 * phases (see hoist_ternaries_in_expr's own doc comment for that
 * established pattern) -- needed so a LATER direct-init in the same
 * block can correctly infer an EARLIER local's type when it's passed
 * as a reference-parameter constructor argument (`Shape a(5); Shape
 * c(a);`, exactly tests/76sample.cpp's own shape). */
static void inject_ctor_calls_block(AstNode *block, AstNode *class_decl, LocalVarType **locals, int *arr_ctor_counter) {
    AstList new_list = ast_list_new();
    for (int i = 0; i < block->list.count; i++) {
        AstNode *stmt = block->list.items[i];
        inject_ctor_calls_stmt(&stmt, class_decl, locals, arr_ctor_counter);
        ast_list_append(&new_list, stmt);

        if (stmt->kind == AST_VAR_DECL && stmt->a == NULL && stmt->type != NULL
            && stmt->type->kind == AST_ARRAY_TYPE
            && stmt->type->a != NULL && stmt->type->a->kind != AST_POINTER_TYPE
            && stmt->type->a->kind != AST_REFERENCE_TYPE) {
            /* `Shape shapes[3];` -- see build_array_ctor_loop's own doc
             * comment for the full story on this branch. Checked ahead
             * of the plain-scalar branch just below (both conditions
             * start with `stmt->a == NULL`, but AST_ARRAY_TYPE and a
             * class-named AST_IDENT are mutually exclusive shapes for
             * stmt->type, so ordering between the two branches doesn't
             * actually matter -- kept first here only because it's the
             * newer, less-established case, easier to spot at the top).
             * The element-type pointer/reference guard is the exact same
             * pre-existing bug fix the scalar branch just below needed
             * (see its own doc comment) -- `Widget *arr[3];` is an array
             * of POINTERS, never itself an array of objects needing
             * per-element construction, but type_to_class resolves
             * straight through a pointer wrapper same as it does for a
             * plain scalar, so without this guard this branch wrongly
             * tried to construct each element as if it were a `Widget`
             * value. */
            AstNode *elem_class = type_to_class(stmt->type->a);
            if (elem_class != NULL) {
                AstNode *loop = build_array_ctor_loop(stmt, elem_class, arr_ctor_counter);
                if (loop != NULL) {
                    ast_list_append(&new_list, loop);
                }
            }
        } else if (stmt->kind == AST_VAR_DECL && stmt->a == NULL
                   && stmt->type != NULL && stmt->type->kind != AST_POINTER_TYPE
                   && stmt->type->kind != AST_REFERENCE_TYPE) {
            /* A real, separate, PRE-EXISTING bug found while adding this
             * phase's own vtable-init fallback just below: type_to_class
             * already resolves straight THROUGH a pointer/reference
             * wrapper (see its own doc comment in sema.c) -- exactly
             * right for every other caller that wants to know "what
             * class does this TYPE ultimately name", but wrong here,
             * where this branch specifically means "this VarDecl is a
             * plain, by-value, uninitialized class object that needs
             * its own constructor called on it". Without this guard, a
             * bare `Widget *p;` (a POINTER local, never itself an
             * object at all) was ALSO treated as needing a constructor
             * call -- confirmed directly against real gcc output
             * generating `Widget__Widget__void((&p));`, calling the
             * constructor with `&p` (a `Widget **`) where the
             * constructor's own `this` parameter is a plain `Widget *`,
             * a real, silently-wrong-code bug (not just a warning) that
             * predates this round's own vtable-init work entirely --
             * this guard closes both at once, since the vtable-init
             * fallback just below would have inherited the identical
             * mistake otherwise. */
            AstNode *var_class = type_to_class(stmt->type);
            if (var_class != NULL) {
                AstNode *ctor = find_zero_arg_constructor(var_class);
                if (ctor != NULL) {
                    FuncSemaInfo *info = (FuncSemaInfo *)ctor->sema_info;
                    const char *mangled = (info != NULL) ? info->mangled_name : ctor->str1;

                    AstNode *addr = ast_new(AST_UNOP, stmt->line);
                    addr->str1 = strdup("addr");
                    addr->a = ast_ident(stmt->str1, stmt->line);

                    /* Same "fill into an empty, receiver-free list
                     * first" ordering as build_array_ctor_loop's own
                     * identical case -- see its doc comment for why. */
                    AstList real_args = ast_list_new();
                    fill_default_args(&real_args, ctor, 1);

                    AstNode *call = ast_new(AST_CALL, stmt->line);
                    call->a = ast_ident(mangled, stmt->line);
                    ast_list_append(&call->list, addr);
                    for (int k = 0; k < real_args.count; k++) {
                        ast_list_append(&call->list, real_args.items[k]);
                    }

                    AstNode *expr_stmt = ast_new(AST_EXPR_STMT, stmt->line);
                    expr_stmt->a = call;
                    ast_list_append(&new_list, expr_stmt);
                } else {
                    /* No user-declared constructor at all -- see
                     * build_vtable_init_stmt's own doc comment for the
                     * real bug this closes: a vtable-owning class with
                     * no constructor previously got NO vtable-pointer
                     * initialization anywhere, for a plain stack local
                     * exactly as much as for `new`. */
                    AstNode *vtable_stmt = build_vtable_init_stmt(
                        ast_ident(stmt->str1, stmt->line), var_class, stmt->line);
                    if (vtable_stmt != NULL) {
                        ast_list_append(&new_list, vtable_stmt);
                    }
                }
            }
        } else if (stmt->kind == AST_VAR_DECL && stmt->a != NULL
                   && stmt->a->kind == AST_DIRECT_INIT) {
            /* Direct-initialization with constructor arguments on a
             * stack local -- `Shape shape(7);` -- the counterpart to the
             * zero-argument case just above, sharing everything except
             * WHICH constructor overload gets called and WHAT arguments
             * it's given. sema.c's check_node already resolved this
             * against `var_class`'s own constructor overloads (attached
             * as a CallResolution on stmt->a->sema_info, exactly the
             * same mechanism resolve_new_expr uses for `new T(args)` --
             * see AST_DIRECT_INIT's own doc comment in ast.h). A NULL
             * resolution here means either the class genuinely has no
             * matching constructor (already reported as a sema error by
             * that point -- nothing more to do) or has no declared
             * constructor at all (not an error -- there's simply nothing
             * to call, mirroring the zero-arg case's own "no ctor
             * exists" no-op just above). */
            AstNode *direct_init = stmt->a;
            CallResolution *cr = (CallResolution *)direct_init->sema_info;
            stmt->a = NULL; /* clear the marker regardless of whether a
                constructor was actually resolved -- codegen has no idea
                what an AST_DIRECT_INIT node is and was never meant to
                see one; leaving it in place would print as this
                VarDecl's own (nonsensical) initializer expression */
            if (cr != NULL && cr->resolved_target != NULL) {
                AstNode *ctor = cr->resolved_target;
                FuncSemaInfo *info = (FuncSemaInfo *)ctor->sema_info;
                const char *mangled = (info != NULL) ? info->mangled_name : ctor->str1;

                AstNode *addr = ast_new(AST_UNOP, stmt->line);
                addr->str1 = strdup("addr");
                addr->a = ast_ident(stmt->str1, stmt->line);

                /* Reference-parameter arguments (a copy constructor's
                 * own `other`, most commonly) already got their implicit
                 * `&` inserted earlier, in finalize_calls_expr's own
                 * AST_DIRECT_INIT case (phase 3/4) -- MUST happen there,
                 * not here: phase 5 (fix_references, already run by this
                 * point) relabels the resolved constructor's own
                 * AST_REFERENCE_TYPE parameter to a plain pointer
                 * in-place on the shared target node, so the identical
                 * "is this parameter still a reference" check would
                 * silently never fire this late -- see
                 * fixup_ctor_reference_args's own doc comment, and
                 * finalize_calls_expr's AST_NEW case's identical
                 * reasoning, for the full account. */

                AstNode *call = ast_new(AST_CALL, stmt->line);
                call->a = ast_ident(mangled, stmt->line);
                ast_list_append(&call->list, addr);
                for (int j = 0; j < direct_init->list.count; j++) {
                    ast_list_append(&call->list, direct_init->list.items[j]);
                }

                AstNode *expr_stmt = ast_new(AST_EXPR_STMT, stmt->line);
                expr_stmt->a = call;
                ast_list_append(&new_list, expr_stmt);
            }
        }

        if (stmt->kind == AST_VAR_DECL) {
            /* Keep `*locals` current for every VarDecl this phase sees
             * (not just the ones needing a constructor call), the same
             * bookkeeping several other phases already do independently
             * for their own purposes (see hoist_ternaries_in_expr's own
             * doc comment) -- needed here so a LATER direct-init in this
             * same block can have an EARLIER local's type correctly
             * inferred when it's passed as a reference-parameter
             * constructor argument. */
            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = stmt->str1;
            lv->type = stmt->type;
            lv->next = *locals;
            *locals = lv;
        }
    }
    block->list = new_list;
}

static void inject_ctor_calls_stmt(AstNode **slot, AstNode *class_decl, LocalVarType **locals, int *arr_ctor_counter) {
    AstNode *s = *slot;
    if (s == NULL) return;
    switch (s->kind) {
        case AST_BLOCK:
            inject_ctor_calls_block(s, class_decl, locals, arr_ctor_counter);
            break;
        case AST_IF:
            inject_ctor_calls_stmt(&s->b, class_decl, locals, arr_ctor_counter);
            inject_ctor_calls_stmt(&s->c, class_decl, locals, arr_ctor_counter);
            break;
        case AST_LABEL:
            inject_ctor_calls_stmt(&s->a, class_decl, locals, arr_ctor_counter);
            break;
        case AST_WHILE:
            inject_ctor_calls_stmt(&s->b, class_decl, locals, arr_ctor_counter);
            break;
        case AST_FOR:
            /* Deliberately NOT recursing into s->a (the for-loop's own
             * init clause) -- see this phase's own doc comment above. */
            inject_ctor_calls_stmt(&s->d, class_decl, locals, arr_ctor_counter);
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
                    if (m->kind == AST_FUNC_DEF) {
                        LocalVarType *locals = seed_locals_from_params(m);
                        int arr_ctor_counter = 0; /* fresh per function, matching
                            __v32_ret_tmpN/__v32_tern_tmpN's own established
                            "per-function counter" convention elsewhere in
                            this file */
                        inject_ctor_calls_stmt(&m->a, n, &locals, &arr_ctor_counter);
                    }
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
            LocalVarType *locals = seed_locals_from_params(n);
            int arr_ctor_counter = 0;
            inject_ctor_calls_stmt(&n->a, NULL, &locals, &arr_ctor_counter);
        }
    }
}

/* ---- phase 8-pre: implicit default construction of class-typed members ---
 *
 * A class-typed data member (`Swarm mSwarm;` inside `Game`) is never
 * constructed today: phase 7 only walks function BODIES for local
 * VarDecls, and a member field is not a local. The member's own fields
 * (and, worse, its vtable pointer) are left as raw stack garbage until
 * first use -- surfaced directly by the real Space Invaders program,
 * where `Game game;` compiled clean but HALTed the CPU the moment a
 * method touched the uninitialized `mSwarm.mGrid` pointers.
 *
 * For every constructor WITH a body, walk the class's own data_members
 * in declaration order and, for each member that is a BARE class type
 * (not pointer/reference/array -- the same guard phase 7's `Widget *p`
 * bug fix established, since type_to_class resolves straight through a
 * pointer wrapper) with a callable-with-zero-arguments constructor,
 * prepend `Member__Member__void(&this->member);` -- with any
 * default-value expressions spliced via fill_default_args, exactly the
 * way build_array_ctor_loop builds the same call shape for stack arrays.
 * A member class with NO constructor but a vtable instead gets
 * `this->member.vtable = &Member_vtable_instance;` prepended -- the
 * member counterpart of build_vtable_init_stmt's own stack-object fix.
 *
 * ORDERING: this phase PREPENDS, so it must run BEFORE phase 8 in the
 * driver -- the phase that prepends last ends up first, and member
 * construction needs to land AFTER base-ctor delegation (8b) and the
 * primitive member-init assigns (8a) in the final body, i.e. this
 * phase's statements must be the first thing prepended. Final emitted
 * order: base-ctor, primitive member assigns, vtable init, member
 * default-construction, user body -- matching real C++'s "base, then
 * members, then body" (one documented approximation: class-typed
 * members are constructed after ALL primitive member-init assigns,
 * not interleaved with them by declaration order).
 *
 * DELIBERATE SCOPE LIMITS, same discipline as phase 7:
 *   - A member already named in the constructor's own member-init list
 *     (`: swarm(40)`) is SKIPPED here -- that member's construction is
 *     the member-init path's job, not this phase's. Once sema.c's
 *     resolve_member_init_list stops rejecting class-typed members,
 *     the two compose without double construction via this check.
 *   - A self-typed member (recursive containment, `Node n;` inside
 *     class Node) is skipped -- it would inject unbounded recursion,
 *     and real C++ rejects the class outright anyway.
 *   - Inherited members are NOT walked -- data_members holds only the
 *     class's OWN fields; the base's members are constructed by the
 *     base's own constructor, which phase 8b already calls.
 *   - Prototype-only member constructors are skipped (no body -- the
 *     emitted call would reference a C function never defined).
 */
static AstNode *build_member_ctor_stmt(AstNode *field, int line) {
    /* receiver: &this->member */
    AstNode *member_ref = ast_new(AST_MEMBER, line);
    member_ref->str1 = strdup("->");
    member_ref->str2 = strdup(field->str1);
    member_ref->a = ast_ident("this", line);

    AstNode *var_class = type_to_class(field->type);
    AstNode *ctor = find_zero_arg_constructor(var_class);

    if (ctor != NULL) {
        FuncSemaInfo *info = (FuncSemaInfo *)ctor->sema_info;
        const char *mangled = (info != NULL) ? info->mangled_name : ctor->str1;

        AstNode *addr = ast_new(AST_UNOP, line);
        addr->str1 = strdup("addr");
        addr->a = member_ref;

        /* fill_default_args expects the argument list WITHOUT the
         * receiver (same convention as build_array_ctor_loop) -- spliced
         * in after, offset 1 past the injected "this". */
        AstList real_args = ast_list_new();
        fill_default_args(&real_args, ctor, 1);

        AstNode *call = ast_new(AST_CALL, line);
        call->a = ast_ident(mangled, line);
        ast_list_append(&call->list, addr);
        for (int k = 0; k < real_args.count; k++) {
            ast_list_append(&call->list, real_args.items[k]);
        }

        AstNode *expr_stmt = ast_new(AST_EXPR_STMT, line);
        expr_stmt->a = call;
        return expr_stmt;
    }

    /* No user-declared constructor at all -- member counterpart of
     * build_vtable_init_stmt: a vtable-owning member class needs its
     * vtable pointer set even with no constructor body to do it.
     * Note "->" vtable access (member reached through this), where
     * build_vtable_init_stmt's stack-object form uses ".". */
    ClassLayout *mlayout = (ClassLayout *)var_class->sema_info;
    if (mlayout == NULL || mlayout->vtable == NULL) return NULL;

    AstNode *vtable_ref = ast_new(AST_MEMBER, line);
    vtable_ref->str1 = strdup(".");
    vtable_ref->str2 = strdup("vtable");
    vtable_ref->a = member_ref;

    size_t len = strlen(var_class->str1) + strlen("_vtable_instance") + 1;
    char *instance_name = malloc(len);
    snprintf(instance_name, len, "%s_vtable_instance", var_class->str1);
    AstNode *addr = ast_new(AST_UNOP, line);
    addr->str1 = strdup("addr");
    addr->a = ast_ident(instance_name, line);
    free(instance_name);

    AstNode *assign = ast_new(AST_ASSIGN, line);
    assign->str1 = strdup("=");
    assign->a = vtable_ref;
    assign->b = addr;

    AstNode *expr_stmt = ast_new(AST_EXPR_STMT, line);
    expr_stmt->a = assign;
    return expr_stmt;
}

/* Is `name` already explicitly initialized in ctor `m`'s member-init
 * list?  (m->c is the MemberInitList -- AstList of AST_MEMBER_INIT,
 * str1 = the member/base name; NULL when no ": ..." was written.) */
static int member_is_explicitly_initialized(AstNode *m, const char *name) {
    if (m->c == NULL) return 0;
    for (int i = 0; i < m->c->list.count; i++) {
        if (strcmp(m->c->list.items[i]->str1, name) == 0) return 1;
    }
    return 0;
}

static void inject_member_ctor_calls_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (strcmp(m->str1, n->str1) != 0) continue; /* not a ctor */
                    if (m->kind != AST_FUNC_DEF) continue;        /* no body */

                    /* Collect this ctor's injected statements FIRST (can't
                     * prepend one at a time with a plain front-insert while
                     * also iterating -- same rebuild-the-list shape phase 8
                     * uses, but appending our statements as a group). */
                    AstList injected = ast_list_new();
                    for (int f = 0; f < layout->data_members.count; f++) {
                        AstNode *field = layout->data_members.items[f];
                        if (field->type == NULL) continue;
                        if (field->type->kind == AST_POINTER_TYPE) continue;
                        if (field->type->kind == AST_REFERENCE_TYPE) continue;
                        if (field->type->kind == AST_ARRAY_TYPE) continue;
                        AstNode *var_class = type_to_class(field->type);
                        if (var_class == NULL) continue;         /* not a class */
                        if (var_class == n) continue;            /* self-recursion */
                        if (member_is_explicitly_initialized(m, field->str1)) continue;
                        AstNode *stmt = build_member_ctor_stmt(field, m->line);
                        if (stmt != NULL) ast_list_append(&injected, stmt);
                    }

                    if (injected.count > 0) {
                        AstNode *body = m->a; /* AST_BLOCK */
                        AstList new_list = ast_list_new();
                        for (int k = 0; k < injected.count; k++) {
                            ast_list_append(&new_list, injected.items[k]);
                        }
                        for (int k = 0; k < body->list.count; k++) {
                            ast_list_append(&new_list, body->list.items[k]);
                        }
                        body->list = new_list;
                    }
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            inject_member_ctor_calls_classes(&n->list);
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
                        /* `: Base(1)` written against a base constructor
                         * that declares MORE parameters than were
                         * written, the rest defaulted (`Base(int a, int
                         * b = 2)`) -- sema.c's own widened arity check
                         * (resolve_overload_generic) already allows this
                         * to resolve; splice the missing trailing
                         * defaults in here, same as everywhere else a
                         * CallResolution against a defaulted constructor
                         * gets acted on. */
                        fill_default_args(&explicit_args, base_ctor, 1);
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
                        /* explicit_args starts empty, but the base's own
                         * constructor may still declare real parameters
                         * that all have defaults (see
                         * find_zero_arg_constructor's own doc comment) --
                         * fill_default_args (offset 1, past the base
                         * ctor's own injected "this") splices those in,
                         * a no-op appending nothing when the base ctor
                         * truly takes no parameters at all. */
                        fill_default_args(&explicit_args, implicit_ctor, 1);
                    }

                    AstNode *receiver = cast_receiver_if_needed(ast_ident("this", m->line), n, layout->base_class_decl, 0 /* this is never const -- see this_inject_method */);

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

/* True when control provably cannot fall past this statement.
 * Deliberately conservative -- only the three shapes this
 * project actually produces that always exit: a bare return, a
 * block whose LAST statement always returns (the shape the
 * return-with-destructibles rewrite itself produces), and an
 * if whose BOTH branches always return. Anything else returns
 * 0, meaning "emit destructors as before". */
static int stmt_always_returns(AstNode *n)
{
    if (n == NULL ) return 0;
    if (n->kind == AST_RETURN ) return 1;
    if (n->kind == AST_BLOCK ) {
        if (n->list.count == 0) return 0;
        return stmt_always_returns(n->list.items[n->list.count - 1]);
    }
    if (n->kind == AST_IF )
        return stmt_always_returns(n->b) && stmt_always_returns(n->c);
    return 0;
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
        case AST_ASM:
            /* Nothing to do: an asm body is opaque string literals --
             * no `this`, member references, or references to rewrite.
             * Passed through to codegen.c verbatim. */
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
                /* NEW: `return 0;` in a pointer-returning function built
                 * `Alien *__v32_ret_tmp0 = 0;` -- the same Vircon32
                 * int-to-pointer rejection; func_return_type is exactly
                 * the target type this initializer flows into (NULL for
                 * void/ctor returns, which the helper treats as "don't
                 * guess"). */
                rewrite_zero_to_null(&tmp_decl->a, func_return_type);

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
     * declaration order -- but SKIPPED entirely when control
     * provably cannot reach the block's end (e.g. the last
     * statement is the always-returning block a return-with-
     * destructibles rewrite just produced). Emitting them there
     * would be unreachable dead code, duplicate with the dtor
     * calls the return path itself already emitted. */
    AstNode *fallthrough_stmt =
        (new_list.count > 0) ? new_list.items[new_list.count - 1] : NULL;
    if (!stmt_always_returns(fallthrough_stmt)) {
        for (DestructibleLocal *dl = this_scope.locals; dl != NULL; dl = dl->next) {
            ast_list_append(&new_list, build_dtor_call_stmt(dl));
        }
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
                        lower_note(n->a->line, "inserted (%s *) upcast for "
                            "`%s`'s initializer -- Vircon32 rejects an "
                            "implicit pointer upcast that real C++ allows "
                            "freely (\"cannot assign struct %s* to struct %s*\")",
                            declared_class->str1, n->str1, init_class->str1, declared_class->str1);
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
 * SCOPE, as it stood before the round documented below: only a ternary
 * that is DIRECTLY the initializer of a var_decl, DIRECTLY the rhs of a
 * plain `=` assignment to a bare identifier, or DIRECTLY a return
 * expression got the no-temp treatment above -- CHAINED that way (the
 * recursion just above), not nested any other way. A ternary nested
 * INSIDE a call argument, as part of a larger arithmetic expression, or
 * assigned through anything other than a bare identifier
 * (`arr[i] = cond ? a : b;`, `obj.field = cond ? a : b;`) fell through
 * completely untouched and did not compile on the real Vircon32
 * toolchain -- confirmed directly: `add(x > y ? x : y, 1)`, a ternary
 * used as a call argument, transpiled with the literal `?`/`:`
 * characters still in it, and the real Vircon32 C lexer doesn't even
 * recognize `?` as a valid token ("character '?' is not a valid
 * identifier start"), so this wasn't merely a missed optimization, it
 * was a straightforward, previously-undiscovered miscompile once a
 * ternary appeared literally anywhere else.
 *
 * FIXED in a later round: `hoist_ternaries_in_expr` (below) generically
 * walks the REMAINING expression shapes this phase's own three direct
 * cases don't already cover (call arguments -- including the callee
 * expression itself, in case it's ever a function-pointer value, binary
 * operators, subscripts, member access, casts, sizeof, `new`'s own
 * constructor arguments and array-size expression, and an assignment to
 * anything other than the bare-identifier shape already handled
 * directly) and hoists any ternary it finds into a freshly-declared
 * temporary, set via an ordinary if/else inserted immediately before
 * the CURRENT statement in its enclosing block -- e.g.
 * `add(x > y ? x : y, 1);` becomes
 * `int __v32_tern_tmp0; if (x > y) { __v32_tern_tmp0 = x; } else {
 * __v32_tern_tmp0 = y; } add(__v32_tern_tmp0, 1);`. Applied ONLY when
 * the statement's own top-level shape ISN'T already one of the three
 * direct, no-temp cases above (so `int x = cond ? a : b;` still gets
 * the cleaner direct rewrite it always did, not an unnecessary temp);
 * recurses post-order into a ternary's own condition/branches first, so
 * a ternary nested inside ANOTHER ternary's own branches (reachable
 * through a call argument or similar, not just the "chained" a?b:c?d:e
 * shape the direct rewrite already handles) is hoisted from the inside
 * out, each one becoming its own preceding temp. The temp's own type is
 * inferred from the ternary's then-branch (falling back to the
 * else-branch, then to `int` as a last-resort default when neither can
 * be determined -- best-effort, matching this project's own established
 * fallback elsewhere rather than leaving the temp's type unresolved).
 *
 * Two boundaries remain, both still real and stated plainly rather than
 * silently missed: (1) a ternary inside a for-loop's own init/cond/incr
 * clauses is still untouched -- those clauses aren't a real statement
 * list to splice extra statements into, and hoisting one would need to
 * restructure the loop itself (e.g. into an equivalent `while`), which
 * this project doesn't attempt; (2) a ternary reachable only through a
 * single, brace-less statement slot (`if (cond) foo(cond2 ? a : b);`
 * with no block around the call) can't be hoisted either, for the exact
 * same structural reason var_decl's own direct rewrite was already
 * documented as unable to reach that shape -- inserting a preceding
 * temp declaration needs a real list to insert into, which only a
 * block provides. Ordinary, brace-using code (the overwhelmingly common
 * style, and the only shape this project's own test suite has ever
 * written) is unaffected by either boundary.
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

/* Generic fallback for every ternary the three direct, no-temp shapes
 * above don't reach -- see this whole phase's own doc comment for the
 * real bug this closes and the reasoning behind hoisting into a temp
 * here specifically. `locals` is threaded through (and grown, via
 * *locals) the same way finalize_calls_stmt's own does: a temp this
 * call introduces is pushed onto it immediately, so a LATER ternary
 * hoisted from the same expression (or a type lookup for one) can see
 * it, and a plain, ordinary var_decl elsewhere in the same block still
 * needs registering by THIS phase's own caller (rewrite_ternary_block)
 * for the exact same reason -- this phase keeps its own, independent
 * locals list rather than sharing finalize_calls_stmt's (phase 3/4's),
 * matching the same "independently reasoned about" principle
 * seed_locals_from_params's own doc comment already states for why
 * fix_references_in_method keeps its own separate list too. */
static void hoist_ternaries_in_expr(AstNode **slot, AstNode *class_decl, LocalVarType **locals,
                                     int *tmp_counter, AstList *out) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_TERNARY: {
            /* Post-order: hoist anything nested inside the condition or
             * either branch FIRST, so each nested ternary becomes its
             * own preceding temp before this one is hoisted into its
             * own -- covers both a plain chained ternary reached this
             * way (rather than through the direct rewrite's own
             * recursion) and one buried behind a call/binop/etc inside
             * a branch. */
            hoist_ternaries_in_expr(&n->a, class_decl, locals, tmp_counter, out);
            hoist_ternaries_in_expr(&n->b, class_decl, locals, tmp_counter, out);
            hoist_ternaries_in_expr(&n->c, class_decl, locals, tmp_counter, out);

            AstNode *ty = infer_expr_type(n->b, class_decl, *locals);
            if (ty == NULL) ty = infer_expr_type(n->c, class_decl, *locals);
            if (ty == NULL) ty = ast_ident("int", n->line); /* best-effort
                default when neither branch's type can be determined --
                see this phase's own doc comment above */

            char tmp_name[40];
            snprintf(tmp_name, sizeof(tmp_name), "__v32_tern_tmp%d", (*tmp_counter)++);

            lower_note(n->line, "hoisted a `?:` ternary into a temporary "
                "(%s) plus an if/else -- Vircon32's lexer doesn't recognize "
                "the '?' character as a valid identifier start at all, so "
                "no ternary can survive to codegen in Vircon32-mode output",
                tmp_name);

            AstNode *decl = ast_new(AST_VAR_DECL, n->line);
            decl->str1 = strdup(tmp_name);
            decl->type = ty;
            ast_list_append(out, decl);

            AstNode *then_assign = ast_new(AST_ASSIGN, n->line);
            then_assign->str1 = strdup("=");
            then_assign->a = ast_ident(tmp_name, n->line);
            then_assign->b = n->b;
            AstNode *then_stmt = ast_new(AST_EXPR_STMT, n->line);
            then_stmt->a = then_assign;

            AstNode *else_assign = ast_new(AST_ASSIGN, n->line);
            else_assign->str1 = strdup("=");
            else_assign->a = ast_ident(tmp_name, n->line);
            else_assign->b = n->c;
            AstNode *else_stmt = ast_new(AST_EXPR_STMT, n->line);
            else_stmt->a = else_assign;

            ast_list_append(out, build_ternary_if_else(n->a, then_stmt, else_stmt, n->line));

            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = decl->str1;
            lv->type = ty;
            lv->next = *locals;
            *locals = lv;

            *slot = ast_ident(tmp_name, n->line);
            break;
        }
        case AST_CALL:
            hoist_ternaries_in_expr(&n->a, class_decl, locals, tmp_counter, out); /* the
                callee itself, in case it's ever a function-pointer VALUE
                expression rather than a plain name -- harmless no-op for
                the ordinary AST_IDENT/AST_MEMBER callee shape */
            for (int i = 0; i < n->list.count; i++) {
                hoist_ternaries_in_expr(&n->list.items[i], class_decl, locals, tmp_counter, out);
            }
            break;
        case AST_BINOP:
        case AST_ASSIGN:
        case AST_SUBSCRIPT:
            hoist_ternaries_in_expr(&n->a, class_decl, locals, tmp_counter, out);
            hoist_ternaries_in_expr(&n->b, class_decl, locals, tmp_counter, out);
            break;
        case AST_MEMBER:
        case AST_UNOP:
        case AST_CAST:
            hoist_ternaries_in_expr(&n->a, class_decl, locals, tmp_counter, out);
            break;
        case AST_SIZEOF:
            hoist_ternaries_in_expr(&n->a, class_decl, locals, tmp_counter, out); /* NULL-safe for the type-taking form */
            break;
        case AST_NEW:
            for (int i = 0; i < n->list.count; i++) {
                hoist_ternaries_in_expr(&n->list.items[i], class_decl, locals, tmp_counter, out);
            }
            hoist_ternaries_in_expr(&n->a, class_decl, locals, tmp_counter, out); /* array-new's own size expression */
            break;
        case AST_DIRECT_INIT:
            /* `Shape shape(cond ? a : b);` -- same reasoning as AST_NEW
             * just above, minus the array-size expression it doesn't
             * have. Reached via the AST_VAR_DECL case in this phase's
             * own block-splicing loop (rewrite_ternary_block), which
             * hands this node in as `stmt->a` exactly like it would any
             * other initializer expression. */
            for (int i = 0; i < n->list.count; i++) {
                hoist_ternaries_in_expr(&n->list.items[i], class_decl, locals, tmp_counter, out);
            }
            break;
        default:
            break;
    }
}

static void rewrite_ternary_block(AstNode *block, AstNode *class_decl, LocalVarType **locals, int *tmp_counter);

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
 * anything a plain slot-replacement can't safely reach -- the same is
 * now true of the generic hoist (this phase's own doc comment states
 * that boundary too). */
static void rewrite_ternary_stmt(AstNode **slot, AstNode *class_decl, LocalVarType **locals, int *tmp_counter) {
    AstNode *s = *slot;
    if (s == NULL) return;
    switch (s->kind) {
        case AST_BLOCK:
            rewrite_ternary_block(s, class_decl, locals, tmp_counter);
            break;
        case AST_IF:
            rewrite_ternary_stmt(&s->b, class_decl, locals, tmp_counter);
            rewrite_ternary_stmt(&s->c, class_decl, locals, tmp_counter);
            break;
        case AST_WHILE:
            rewrite_ternary_stmt(&s->b, class_decl, locals, tmp_counter);
            break;
        case AST_FOR:
            rewrite_ternary_stmt(&s->d, class_decl, locals, tmp_counter);
            break;
        case AST_LABEL:
            rewrite_ternary_stmt(&s->a, class_decl, locals, tmp_counter);
            break;
        case AST_RETURN:
            if (s->a != NULL && s->a->kind == AST_TERNARY) {
                lower_note(s->line, "rewrote `return cond ? a : b;` into an "
                    "if/else returning from each branch -- Vircon32's lexer "
                    "doesn't recognize '?' at all, so no ternary can reach codegen");
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
                rewrite_ternary_stmt(&then_ret, class_decl, locals, tmp_counter);
                rewrite_ternary_stmt(&else_ret, class_decl, locals, tmp_counter);
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

                lower_note(s->line, "rewrote `%s = cond ? a : b;` into an "
                    "if/else assigning each branch -- Vircon32's lexer "
                    "doesn't recognize '?' at all, so no ternary can reach codegen",
                    name);

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
                rewrite_ternary_stmt(&then_stmt, class_decl, locals, tmp_counter);
                rewrite_ternary_stmt(&else_stmt, class_decl, locals, tmp_counter);
                *slot = build_ternary_if_else(t->a, then_stmt, else_stmt, s->line);
            }
            break;
        default:
            break;
    }
}

static void rewrite_ternary_block(AstNode *block, AstNode *class_decl, LocalVarType **locals, int *tmp_counter) {
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
            rewrite_ternary_stmt(&then_stmt, class_decl, locals, tmp_counter);
            rewrite_ternary_stmt(&else_stmt, class_decl, locals, tmp_counter);

            ast_list_append(&new_list, build_ternary_if_else(t->a, then_stmt, else_stmt, stmt->line));

            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = decl_only->str1;
            lv->type = decl_only->type;
            lv->next = *locals;
            *locals = lv;
            continue;
        }

        /* Generic fallback: hoist any ternary reachable from this
         * statement's own top-level expression slot(s) that ISN'T
         * already one of the three direct, no-temp shapes handled
         * above/below -- see this whole phase's own doc comment for
         * exactly what this covers (a call argument, arithmetic, an
         * if/while condition, an assignment to anything other than a
         * bare identifier) and the real bug it closes. Skipped
         * entirely for a slot that IS already a direct-shape ternary
         * (checked by kind alone -- cheap, and avoids hoisting into a
         * needless temp for the common case rewrite_ternary_stmt,
         * called below, already rewrites more cleanly with none). */
        AstList hoisted = ast_list_new();
        switch (stmt->kind) {
            case AST_VAR_DECL:
                if (stmt->a != NULL && stmt->a->kind != AST_TERNARY) {
                    hoist_ternaries_in_expr(&stmt->a, class_decl, locals, tmp_counter, &hoisted);
                }
                break;
            case AST_RETURN:
                if (stmt->a != NULL && stmt->a->kind != AST_TERNARY) {
                    hoist_ternaries_in_expr(&stmt->a, class_decl, locals, tmp_counter, &hoisted);
                }
                break;
            case AST_EXPR_STMT:
                if (!(stmt->a != NULL && stmt->a->kind == AST_ASSIGN
                      && stmt->a->str1 != NULL && strcmp(stmt->a->str1, "=") == 0
                      && stmt->a->a != NULL && stmt->a->a->kind == AST_IDENT
                      && stmt->a->b != NULL && stmt->a->b->kind == AST_TERNARY)) {
                    hoist_ternaries_in_expr(&stmt->a, class_decl, locals, tmp_counter, &hoisted);
                }
                break;
            case AST_IF:
                hoist_ternaries_in_expr(&stmt->a, class_decl, locals, tmp_counter, &hoisted);
                break;
            case AST_WHILE:
                hoist_ternaries_in_expr(&stmt->a, class_decl, locals, tmp_counter, &hoisted);
                break;
            /* AST_FOR's own init/cond/incr clauses, and any other
             * statement kind, are left alone here -- see this phase's
             * own doc comment for the stated boundary on for-loop
             * clauses; every other kind either has no top-level
             * expression of its own (AST_BLOCK, AST_LABEL) or is
             * handled by rewrite_ternary_stmt's own recursion below. */
            default:
                break;
        }
        for (int j = 0; j < hoisted.count; j++) {
            ast_list_append(&new_list, hoisted.items[j]);
        }

        rewrite_ternary_stmt(&stmt, class_decl, locals, tmp_counter);
        ast_list_append(&new_list, stmt);

        if (stmt->kind == AST_VAR_DECL) {
            /* Keep the locals list current for every OTHER var_decl
             * shape too (not just the direct-ternary one handled
             * above), so a LATER ternary hoisted from elsewhere in
             * this same block can resolve this one's type -- mirrors
             * finalize_calls_stmt's own bookkeeping (phase 3/4), kept
             * as this phase's own separate list for the same
             * "independently reasoned about" reason given on
             * hoist_ternaries_in_expr's own doc comment above. */
            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = stmt->str1;
            lv->type = stmt->type;
            lv->next = *locals;
            *locals = lv;
        }
    }
    block->list = new_list;
}

static void rewrite_ternary_in_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return;
    LocalVarType *locals = seed_locals_from_params(method);
    int tmp_counter = 0;
    rewrite_ternary_stmt(&method->a, class_decl, &locals, &tmp_counter);
}

static void rewrite_ternary_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    rewrite_ternary_in_method(layout->methods.items[j], n);
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
            rewrite_ternary_in_method(n, NULL);
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
    /* By-value NATIVE params/returns are not checked here: sema.c's
     * check_native_signature already rejected them (as errors, once per
     * function) before lowering ever runs. A native type has no
     * StructLayout, so bare_class_type() below returns NULL for it and
     * this word-size warning correctly stays silent. */
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
    lower_notes_reset(); /* always start this run's log empty -- see
        lower_notes_print's own doc comment */
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
    inject_member_ctor_calls_classes(&program->list); /* phase 8-pre -- MUST run
        before phase 8: both prepend, and member construction has to end up
        AFTER base-ctor (8b) / member-assigns (8a) / vtable-init (8) in the
        final body, so this phase prepends FIRST of the five -- see its own
        doc comment */
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
