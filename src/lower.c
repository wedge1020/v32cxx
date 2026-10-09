#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lower.h"
#include "generic.h"
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
        case AST_SWITCH:
            /* A switch body is a flat statement list with case/default
             * labels mixed in -- walked exactly like a block's. (Missing
             * from this walker until 20261001: calls, this->, references,
             * new/delete, constructors and casts inside case bodies all
             * went unlowered.) */
            rewrite_expr(&n->a, class_decl, *locals);
            for (int i = 0; i < n->list.count; i++)
                rewrite_stmt(&n->list.items[i], class_decl, locals);
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
    /* A reference member (`int &r;`) is not made const by a const
     * object -- what it refers to keeps its own constness, as in C++ --
     * and phase 5 turns its reads into `(*this->r)`, a plain int. */
    if (read_type->kind == AST_REFERENCE_TYPE) return expr;
    /* A struct can't be cast by value (`(Vec)this->pos` is not C);
     * phase 11 reads a const struct through a pointer cast instead. */
    if (type_to_class(read_type) != NULL) return expr;
    /* Nor can an array (`(char [64])this->m_data` is not C either): an
     * array member read through a const object is left as it is, and
     * decays to a pointer to const like any other. */
    if (read_type->kind == AST_ARRAY_TYPE) return expr;
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
static int is_deref_unop(const AstNode *n);

static int needs_reference_temporary(const AstNode *arg, const AstNode *param_type);
static AstNode *bind_reference_temporary(AstNode *arg, AstNode *param_type);

/* address_of_if_needed serves two callers with slightly different needs:
 *   - a method's RECEIVER wants a pointer to the object, so an expression
 *     that already is a pointer is left alone (g_ref_binding_type NULL);
 *   - BINDING A REFERENCE (an argument, a `return`, a reference local)
 *     wants the address of whatever the expression names -- including
 *     when that is itself a pointer and the reference is to a pointer
 *     (`Shape *&`, which std::vector<Shape *> produces). The caller sets
 *     g_ref_binding_type to the reference type being bound for the
 *     duration of the call. */
static const AstNode *g_ref_binding_type = NULL;

static int reference_to_pointer(const AstNode *ref_type) {
    if (ref_type == NULL || ref_type->kind != AST_REFERENCE_TYPE) return 0;
    const AstNode *t = ref_type->a;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    t = resolve_typedef_chain(t);
    return t != NULL && t->kind == AST_POINTER_TYPE;
}

static AstNode *address_of_if_needed(AstNode *obj_expr, AstNode *class_decl, LocalVarType *locals) {
    int binding_pointer_ref = reference_to_pointer(g_ref_binding_type);
    if (g_ref_binding_type == NULL) {
        /* Receiver: a pointer reached through a typedef (`typedef Shape
         * *ShapeP; ShapeP p; p->area();`) is a pointer all the same -- it
         * was being given a `&`, as if `p` were the object. Checked
         * before the dereference shortcut below, which would otherwise
         * strip the `*` from `(*call)->area()` where the call returns a
         * reference to such a pointer. */
        const AstNode *t0 = resolve_typedef_chain(infer_expr_type(obj_expr, class_decl, locals));
        if (t0 != NULL && t0->kind == AST_POINTER_TYPE) return obj_expr;
    }
    if (is_deref_unop(obj_expr)) {
        /* `&*p` is just `p`. This is what a reference-returning call
         * looks like once its dereference has been inserted
         * (`grid[1].fill(2)`: the receiver is `*op_index(...)`), and
         * equally a user-written `(*p).method()`. */
        return obj_expr->a;
    }
    AstNode *t = infer_expr_type(obj_expr, class_decl, locals);
    if (t == NULL || (t->kind == AST_POINTER_TYPE && !binding_pointer_ref) || t->kind == AST_REFERENCE_TYPE) {
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
        if (t != NULL && t->kind == AST_REFERENCE_TYPE &&
            (obj_expr->kind == AST_IDENT || obj_expr->kind == AST_MEMBER)) {
            /* This reference identifier is wanted as the POINTER it
             * lowers to -- tell phase 5 not to dereference it (see
             * ref_as_ptr in ast.h). */
            obj_expr->ref_as_ptr = 1;
        }
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
            if (needs_reference_temporary(args->items[i], param->type)) {
                args->items[i] = bind_reference_temporary(args->items[i], param->type);
                continue;
            }
            AstNode *arg_static_type = infer_expr_type(args->items[i], class_decl, locals);
            g_ref_binding_type = param->type;
            args->items[i] = address_of_if_needed(args->items[i], class_decl, locals);
            g_ref_binding_type = NULL;
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
    /* b, c and d too: the right side of an assignment or binary operator
     * (`area += shapes[i]->area();`) was never searched, so the temporary
     * was declared and then not used -- the receiver was still evaluated
     * at each of its original places. */
    if (n->b == target) n->b = ast_ident(name, line);
    else replace_node_identity(n->b, target, name, line);
    if (n->c == target) n->c = ast_ident(name, line);
    else replace_node_identity(n->c, target, name, line);
    if (n->d == target) n->d = ast_ident(name, line);
    else replace_node_identity(n->d, target, name, line);
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
    if (decl->type == NULL && expr->kind == AST_CAST) decl->type = expr->type;
    if (decl->type == NULL) return NULL; /* type unknown: the caller leaves
        the expression where it is rather than declare an untyped temporary */
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
                    if (decl != NULL) {
                        replace_node_identity(stmt, recv, name, expr->line);
                        ast_list_insert_at(block_list, base_index + *inserted, decl);
                        (*inserted)++;
                    }
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
                        if (decl != NULL) {
                            replace_node_identity(stmt, arg, name, expr->line);
                            ast_list_insert_at(block_list, base_index + *inserted, decl);
                            (*inserted)++;
                        }
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

/* ---- temporaries for reference parameters ------------------------------
 *
 * `fill(7)` where the parameter is `const int &value`: a reference needs
 * something to point AT, and a literal (or `a + b`, or the result of a
 * call) has no address -- the generated C used to be `fill((&7))`. C++
 * creates an unnamed temporary; so does this:
 *
 *     int __v32_ref_tmp0;                       // top of the function
 *     ...
 *     fill((__v32_ref_tmp0 = 7, &__v32_ref_tmp0));
 *
 * The comma form is valid standard C as it stands; for Vircon32, which
 * has no comma operator, phase 10 turns it into statements.
 *
 * Scalars and pointers only. A class-typed temporary would need its
 * constructor and destructor run, and a struct returned by value already
 * gets its own temporary from the by-value ABI phase.
 */
static AstList g_ref_temps;          /* declarations owed to the current function */
static int g_ref_temp_counter = 0;

static const AstNode *reference_referent(const AstNode *ref_type) {
    const AstNode *t = ref_type->a;
    while (t != NULL && t->kind == AST_CONST_TYPE) t = t->a;
    return t;
}

static int needs_reference_temporary(const AstNode *arg, const AstNode *param_type) {
    const AstNode *referent = reference_referent(param_type);
    if (referent == NULL || referent->kind == AST_ARRAY_TYPE) return 0;
    const AstNode *resolved = resolve_typedef_chain(referent);
    if (resolved != NULL && resolved->kind != AST_POINTER_TYPE && type_to_class(resolved) != NULL) return 0;
    switch (arg->kind) {
        case AST_INT_LIT: case AST_FLOAT_LIT: case AST_CHAR_LIT: case AST_BOOL_LIT:
        case AST_NULL_LIT: case AST_STRING_LIT: case AST_SIZEOF: case AST_CAST:
        case AST_TERNARY:
        case AST_NEW:   /* `bombs.push_back(new Bullet(x, y));` */
            return 1;
        case AST_BINOP:
            return 1; /* arithmetic, comparison: always a value */
        case AST_UNOP:
            /* `*p` is an lvalue; `&x`, `-x`, `!x` are values */
            return arg->str1 == NULL || strcmp(arg->str1, "deref") != 0;
        case AST_CALL: {
            const CallResolution *cr = (const CallResolution *)arg->sema_info;
            if (cr == NULL || cr->resolved_target == NULL) return 0;
            const AstNode *rt = cr->resolved_target->type;
            if (rt == NULL || rt->kind == AST_REFERENCE_TYPE) return 0;
            return rt->kind == AST_POINTER_TYPE || type_to_class(rt) == NULL;
        }
        default:
            return 0; /* identifiers, members, subscripts: real lvalues */
    }
}

static AstNode *bind_reference_temporary(AstNode *arg, AstNode *param_type) {
    char name[64];
    snprintf(name, sizeof name, "__v32_ref_tmp%d", g_ref_temp_counter++);
    AstNode *decl = ast_new(AST_VAR_DECL, arg->line);
    decl->str1 = strdup(name);
    decl->type = (AstNode *)reference_referent(param_type); /* shared, like every type node */
    ast_list_append(&g_ref_temps, decl);

    AstNode *assign = ast_new(AST_ASSIGN, arg->line);
    assign->str1 = strdup("=");
    assign->a = ast_ident(name, arg->line);
    assign->b = arg;
    if (decl->type->kind == AST_POINTER_TYPE && arg->kind == AST_INT_LIT && arg->ival == 0) {
        /* `slots.fill(0);` into a pointer element: Vircon32 C wants NULL,
         * not 0, for a pointer (the same rewrite a plain `p = 0;` gets). */
        assign->b = ast_new(AST_NULL_LIT, arg->line);
    }
    if (decl->type->kind == AST_POINTER_TYPE && arg->kind != AST_NULL_LIT && arg->kind != AST_INT_LIT) {
        /* A pointer temporary (`shapes.push_back(&square)` into a
         * `Shape *const &`): Vircon32 C wants the derived-to-base
         * conversion spelled out, so the value is cast to the
         * temporary's own type. */
        AstNode *cast = ast_new(AST_CAST, arg->line);
        cast->type = decl->type;
        cast->a = arg;
        assign->b = cast;
    }
    AstNode *addr = ast_new(AST_UNOP, arg->line);
    addr->str1 = strdup("addr");
    addr->a = ast_ident(name, arg->line);
    AstNode *comma = ast_new(AST_BINOP, arg->line);
    comma->str1 = strdup(",");
    comma->a = assign;
    comma->b = addr;
    lower_note(arg->line, "a temporary (%s) holds the value bound to a reference parameter", name);
    return comma;
}

/* Declares, at the top of `func`'s body, every temporary its call sites
 * asked for. */
static void flush_reference_temporaries(AstNode *func) {
    if (g_ref_temps.count > 0 && func->a != NULL && func->a->kind == AST_BLOCK) {
        AstList body = ast_list_new();
        for (int i = 0; i < g_ref_temps.count; i++) ast_list_append(&body, g_ref_temps.items[i]);
        for (int i = 0; i < func->a->list.count; i++) ast_list_append(&body, func->a->list.items[i]);
        func->a->list = body;
    }
    g_ref_temps = ast_list_new();
    g_ref_temp_counter = 0;
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
            if (param->type != NULL && param->type->kind == AST_REFERENCE_TYPE &&
                needs_reference_temporary(call->list.items[i], param->type)) {
                call->list.items[i] = bind_reference_temporary(call->list.items[i], param->type);
            } else if (param->type != NULL && param->type->kind == AST_REFERENCE_TYPE) {
                AstNode *arg_static_type = infer_expr_type(call->list.items[i], class_decl, locals);
                g_ref_binding_type = param->type;
                call->list.items[i] = address_of_if_needed(call->list.items[i], class_decl, locals);
                g_ref_binding_type = NULL;
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

/* ---- inlined container accessors ------------------------------------------
 *
 * `v[i]` on a std::vector or std::array resolves, like any overloaded
 * operator, to a function call: vector_int__op_index__int(&v, i), then a
 * dereference of the pointer it returns. Correct, and far too slow for
 * the inner loop of a game on this console -- a call, a return and a
 * temporary for every element touched. The transpiler wrote these
 * classes itself (generic.c) and knows exactly what each accessor does,
 * so it writes the body in place of the call:
 *
 *     v[i]  v.at(i)      v.m_data[i]
 *     v.front()          v.m_data[0]
 *     v.back()           v.m_data[v.m_size - 1]      (array: m_data[N - 1])
 *     v.size()           v.m_size                    (array: N)
 *     v.capacity()       v.m_capacity
 *     v.empty()          (v.m_size == 0)             (array: false)
 *     v.data() v.begin() v.m_data
 *     v.end()            (v.m_data + v.m_size)       (array: &m_data[N])
 *
 * Everything that changes the container (push_back, erase, resize, ...)
 * stays a call. An accessor that would have to mention the container
 * twice, or drop it, is only inlined when the container expression has
 * no call in it (`makeList().back()` keeps its call, so makeList() still
 * runs exactly once). --no-inline-containers turns all of this off.
 *
 * Runs on a call finalize_call has already put in its final shape:
 * callee = the mangled name, first argument = the receiver's address.
 * Returns 1 if *slot was replaced. For the reference-returning accessors
 * the replacement IS the element, so the caller must not add the
 * dereference a reference return otherwise gets.
 */
static AstNode *array_length_expr(const AstNode *array_type, int line);

static AstNode *container_field(AstNode *recv_ptr, const char *field, int line) {
    AstNode *m = ast_new(AST_MEMBER, line);
    m->str2 = strdup(field);
    if (recv_ptr->kind == AST_UNOP && recv_ptr->str1 != NULL && strcmp(recv_ptr->str1, "addr") == 0) {
        m->str1 = strdup(".");          /* (&v)->m_size  is  v.m_size */
        m->a = recv_ptr->a;
    } else {
        m->str1 = strdup("->");
        m->a = recv_ptr;
    }
    return m;
}

static AstNode *container_int(int value, int line) {
    AstNode *lit = ast_new(AST_INT_LIT, line);
    lit->ival = value;
    return lit;
}

static AstNode *container_index(AstNode *array, AstNode *index, int line) {
    AstNode *sub = ast_new(AST_SUBSCRIPT, line);
    sub->a = array;
    sub->b = index;
    return sub;
}

static AstNode *container_binop(const char *op, AstNode *a, AstNode *b, int line) {
    AstNode *n = ast_new(AST_BINOP, line);
    n->str1 = strdup(op);
    n->a = a;
    n->b = b;
    return n;
}

static int inline_container_accessor(AstNode **slot, const AstNode *class_decl) {
    AstNode *call = *slot;
    /* Not inside the generated classes themselves: their own methods
     * are the reference the inlined forms are copied from, and a
     * `max_size() { return size(); }` reduced to `return 8;` would no
     * longer use `this` (an "unused argument" warning per method from
     * the Vircon32 C compiler). */
    if (class_decl != NULL && generic_class_kind(class_decl->str1) != 0) return 0;
    if (g_no_inline_containers || call == NULL || call->kind != AST_CALL) return 0;
    CallResolution *cr = (CallResolution *)call->sema_info;
    if (cr == NULL || cr->resolved_target == NULL) return 0;
    AstNode *target = cr->resolved_target;
    if (target->ival == 1 || call->a == NULL || call->a->kind != AST_IDENT || call->list.count < 1) return 0;

    /* which class is this a method of?  "<class>__<method>__<params>" */
    FuncSemaInfo *info = (FuncSemaInfo *)target->sema_info;
    if (info == NULL || info->mangled_name == NULL || target->str1 == NULL) return 0;
    const char *sep = strstr(info->mangled_name, "__");
    if (sep == NULL) return 0;
    char class_name[256];
    size_t len = (size_t)(sep - info->mangled_name);
    if (len == 0 || len >= sizeof class_name) return 0;
    memcpy(class_name, info->mangled_name, len);
    class_name[len] = '\0';
    int kind = generic_class_kind(class_name);
    if (kind == 0) return 0;
    int is_vector = (kind == 2);

    const char *name = target->str1;
    int nargs = call->list.count - 1;
    int line = call->line;
    AstNode *recv = call->list.items[0];
    int simple = !expr_contains_call(recv);

    /* a std::array's length, as written in its m_data declaration */
    AstNode *length = NULL;
    if (!is_vector) {
        AstNode *ident = ast_ident(class_name, line);
        AstNode *cls = type_to_class(ident);
        ClassLayout *layout = (cls != NULL) ? (ClassLayout *)cls->sema_info : NULL;
        if (layout == NULL) return 0;
        for (int i = 0; i < layout->data_members.count; i++) {
            AstNode *f = layout->data_members.items[i];
            if (strcmp(f->str1, "m_data") == 0 && f->type != NULL && f->type->kind == AST_ARRAY_TYPE)
                length = ast_clone_expr(array_length_expr(f->type, line));
        }
        if (length == NULL) return 0;
    }

    AstNode *out = NULL;
    if ((strcmp(name, "operator[]") == 0 || strcmp(name, "at") == 0) && nargs == 1) {
        out = container_index(container_field(recv, "m_data", line), call->list.items[1], line);
    } else if (nargs != 0) {
        return 0;
    } else if (strcmp(name, "front") == 0) {
        out = container_index(container_field(recv, "m_data", line), container_int(0, line), line);
    } else if (strcmp(name, "capacity") == 0 && is_vector) {
        out = container_field(recv, "m_capacity", line);
    } else if ((strcmp(name, "data") == 0 || strcmp(name, "begin") == 0) && is_vector) {
        out = container_field(recv, "m_data", line);
    } else if (strcmp(name, "size") == 0 && is_vector) {
        out = container_field(recv, "m_size", line);
    } else if (strcmp(name, "empty") == 0 && is_vector) {
        out = container_binop("==", container_field(recv, "m_size", line), container_int(0, line), line);
    } else if (!simple) {
        return 0; /* everything below mentions the container twice, or not at all */
    } else if (strcmp(name, "back") == 0 && is_vector) {
        AstNode *last = container_binop("-", container_field(ast_clone_expr(recv), "m_size", line),
                                        container_int(1, line), line);
        out = container_index(container_field(recv, "m_data", line), last, line);
    } else if (strcmp(name, "end") == 0 && is_vector) {
        out = container_binop("+", container_field(recv, "m_data", line),
                              container_field(ast_clone_expr(recv), "m_size", line), line);
    } else if (is_vector) {
        return 0;
    } else if (strcmp(name, "size") == 0 || strcmp(name, "max_size") == 0) {
        out = length;
    } else if (strcmp(name, "empty") == 0) {
        out = ast_new(AST_BOOL_LIT, line);
        out->ival = 0;
    } else if (strcmp(name, "back") == 0) {
        out = container_index(container_field(recv, "m_data", line),
                              container_binop("-", length, container_int(1, line), line), line);
    } else if (strcmp(name, "data") == 0 || strcmp(name, "begin") == 0) {
        out = container_field(recv, "m_data", line); /* the array itself: it decays to `T *` */
    } else if (strcmp(name, "end") == 0) {
        AstNode *addr = ast_new(AST_UNOP, line);
        addr->str1 = strdup("addr");
        addr->a = container_index(container_field(recv, "m_data", line), length, line);
        out = addr;
    } else {
        return 0;
    }
    out->file = call->file;
    *slot = out;
    return 1;
}

/* Wraps the call in *slot in a dereference when its resolved target
 * returns a C++ reference (a pointer, once lowered): a reference return
 * acts like the referent itself. Shared by ordinary calls and by
 * overloaded-operator calls -- the latter never got this before, so
 * `v[0] = 5` through an `int &operator[]` assigned to the returned
 * POINTER instead of through it. */
static void deref_reference_return(AstNode **slot) {
    AstNode *n = *slot;
    CallResolution *cr = (CallResolution *)n->sema_info;
    if (cr == NULL || cr->resolved_target == NULL) return;
    AstNode *target = cr->resolved_target;
    if (target->type == NULL || target->type->kind != AST_REFERENCE_TYPE) return;
    AstNode *deref = ast_new(AST_UNOP, n->line);
    deref->str1 = strdup("deref");
    deref->a = n;
    *slot = deref;
}

static int is_deref_unop(const AstNode *n) {
    return n != NULL && n->kind == AST_UNOP && n->str1 != NULL && strcmp(n->str1, "deref") == 0;
}

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
    if (inline_container_accessor(slot, class_decl)) return;
    deref_reference_return(slot);
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
    if (expr->kind == AST_QUALIFIED_ID)
        collect_qualified_free_function_candidates(expr, &candidates, &count, &cap);
    else
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
    if (!type_is_plain_pointer(target_type) && !type_is_func_ptr(target_type)) return;
        /* function pointers too: `fn = 0;` / `fn != 0` are just as
         * rejected ("cannot assign int to ... void(int*)*") */
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
             * above; candidates come from the qualifier's namespace only
             * (collect_qualified_free_function_candidates -- see sema.c's
             * "namespace-aware free-function lookup"). A local cannot hide
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
            collect_qualified_free_function_candidates(n, &candidates, &count, &cap);
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
            /* A function passed as a callback: `apply( add, a, b )`. Same
             * `&` the assignment and initializer paths already insert --
             * Vircon32 C does not turn a bare function name into a
             * pointer ("cannot assign void(...) to ... void(...)*"). Done
             * before the arguments are finalized, while the name is
             * still the one the user wrote. */
            for (int i = 0; i < n->list.count; i++) {
                if (is_bare_free_function_ref(n->list.items[i], locals)) {
                    wrap_addr_of(&n->list.items[i]);
                }
            }
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
             * A reference-typed local bound to such a call
             * (`int &r = obj.getRef();`) wants the pointer back, not
             * the value: finalize_calls_stmt's AST_VAR_DECL case strips
             * this dereference again for exactly that declaration. */
            if (inline_container_accessor(slot, class_decl)) break;
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
                if (type_is_plain_pointer(a_type) || type_is_func_ptr(a_type)) {
                    rewrite_zero_to_null(&n->b, a_type);
                } else if (type_is_plain_pointer(b_type) || type_is_func_ptr(b_type)) {
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
            /* n->b: postfix ++/--'s dummy int argument, set by sema only
             * when an operator++(int) / operator--(int) resolved */
            rewrite_operator_use(slot, n->a, n->b, class_decl, locals);
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
        case AST_SWITCH:
            /* A switch body is a flat statement list with case/default
             * labels mixed in -- walked exactly like a block's. (Missing
             * from this walker until 20261001: calls, this->, references,
             * new/delete, constructors and casts inside case bodies all
             * went unlowered.) */
            finalize_calls_expr(&n->a, class_decl, *locals);
            for (int i = 0; i < n->list.count; i++)
                finalize_calls_stmt(&n->list.items[i], class_decl, locals);
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
            /* A reference-returning call used purely as a statement
             * (`a += b;` through `T &operator+=`, `obj.chain();`): drop
             * the dereference, which would only load a value nobody
             * reads. */
            if (is_deref_unop(n->a) && n->a->a != NULL && n->a->a->kind == AST_CALL) {
                n->a = n->a->a;
            }
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
            /* A reference-typed local binds to its initializer's ADDRESS
             * (`int &r = n;` -> `int *r = &n;`). An initializer that is
             * already a dereference -- a reference-returning call, or a
             * user-written `*p` -- binds to the pointer underneath. */
            if (n->type != NULL && n->type->kind == AST_REFERENCE_TYPE && n->a != NULL) {
                if (is_deref_unop(n->a)) {
                    n->a = n->a->a;
                } else {
                    g_ref_binding_type = n->type;
                    n->a = address_of_if_needed(n->a, class_decl, *locals);
                    g_ref_binding_type = NULL;
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
static const AstNode *g_ref_return_type = NULL; /* the enclosing function's `T &` */

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
        case AST_SWITCH:
            /* A switch body is a flat statement list with case/default
             * labels mixed in -- walked exactly like a block's. (Missing
             * from this walker until 20261001: calls, this->, references,
             * new/delete, constructors and casts inside case bodies all
             * went unlowered.) */
            for (int i = 0; i < n->list.count; i++)
                inject_reference_return_address_stmt(&n->list.items[i], class_decl, locals);
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
                g_ref_binding_type = g_ref_return_type;
                n->a = address_of_if_needed(n->a, class_decl, *locals);
                g_ref_binding_type = NULL;
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
    /* The arguments of a member-initializer list are expressions like
     * any other (`: mCooldown(pickDelay())`, `: mPos(start.x, 0)`): their
     * calls need finalizing too. They were skipped, so a call in one
     * kept its unmangled source name. */
    if (method->c != NULL) {
        for (int i = 0; i < method->c->list.count; i++) {
            AstNode *entry = method->c->list.items[i];
            for (int j = 0; j < entry->list.count; j++)
                finalize_calls_expr(&entry->list.items[j], class_decl, locals);
        }
    }
    finalize_calls_stmt(&method->a, class_decl, &locals);
    flush_reference_temporaries(method);
    if (method->type != NULL && method->type->kind == AST_REFERENCE_TYPE) {
        LocalVarType *ret_locals = seed_locals_from_params(method);
        g_ref_return_type = method->type;
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
            /* Free-function name lookup inside this body is relative to
             * the namespace it's in -- see sema.c's "namespace-aware
             * free-function lookup". */
            const char *saved = sema_get_lookup_namespace();
            char *inner = sema_ns_join(saved, n->str1);
            sema_set_lookup_namespace(inner);
            finalize_calls_classes(&n->list);
            sema_set_lookup_namespace(saved);
            free(inner);
        }
    }
}

static void finalize_calls_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            /* Free-function name lookup inside this body is relative to
             * the namespace it's in -- see sema.c's "namespace-aware
             * free-function lookup". */
            const char *saved = sema_get_lookup_namespace();
            char *inner = sema_ns_join(saved, n->str1);
            sema_set_lookup_namespace(inner);
            finalize_calls_free_functions(&n->list);
            sema_set_lookup_namespace(saved);
            free(inner);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            /* n->b == NULL: a genuine free function, not an out-of-line
             * method's top-level duplicate (see attach_out_of_line and
             * the identical guard already used elsewhere in this
             * project for exactly this reason). */
            LocalVarType *locals = seed_locals_from_params(n);
            g_v32_temp_counter = 0;                        /* NEW: phase 3c */
            finalize_calls_stmt(&n->a, NULL, &locals);
            flush_reference_temporaries(n);
            if (n->type != NULL && n->type->kind == AST_REFERENCE_TYPE) {
                /* Same reference-return handling as
                 * finalize_calls_in_method just above -- a free function
                 * can return T& just as much as a method can. */
                LocalVarType *ret_locals = seed_locals_from_params(n);
                g_ref_return_type = n->type;
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

/* Phase 5's view of reference DATA members: the class whose method is
 * being walked (NULL in a free function), and whether any class in the
 * program has one at all (so the lookup costs nothing otherwise). */
static AstNode *g_fix_ref_class = NULL;
static int g_fix_ref_any_members = 0;

/* `int &r;` as a class member is a pointer in C: relabel every such
 * member's type once, before any body is walked, and mark it. */
static void relabel_reference_members(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout == NULL) continue;
            for (int j = 0; j < layout->data_members.count; j++) {
                AstNode *m = layout->data_members.items[j];
                if (m->type != NULL && m->type->kind == AST_REFERENCE_TYPE) {
                    m->type->kind = AST_POINTER_TYPE;
                    m->is_ref_member = 1;
                    g_fix_ref_any_members = 1;
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            relabel_reference_members(&n->list);
        }
    }
}

static void fix_reference_access_expr(AstNode **slot, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_IDENT: {
            /* A reference identifier used as a VALUE (`a = a + 1`,
             * `dst = src`, `int b = a`) means the referent: once the
             * reference is a pointer, that is `(*a)`. The uses that want
             * the pointer itself were flagged by address_of_if_needed
             * (ref_as_ptr); member access is handled by AST_MEMBER below. */
            if (n->str1 != NULL && !n->ref_as_ptr) {
                LocalVarType *lv = find_local(locals, n->str1);
                if (lv != NULL && lv->was_reference) {
                    AstNode *deref = ast_new(AST_UNOP, n->line);
                    deref->str1 = strdup("deref");
                    deref->a = n;
                    *slot = deref;
                }
            }
            break;
        }
        case AST_MEMBER: {
            int converted = 0;
            if (n->str1 != NULL && strcmp(n->str1, ".") == 0 && n->a != NULL && n->a->kind == AST_IDENT) {
                LocalVarType *lv = find_local(locals, n->a->str1);
                if (lv != NULL && lv->was_reference) {
                    free(n->str1);
                    n->str1 = strdup("->");
                    converted = 1; /* `r.x` -> `r->x`: the identifier stays a pointer */
                }
            }
            if (!converted) fix_reference_access_expr(&n->a, locals);
            /* A reference DATA MEMBER (`int &r;`, now a pointer) used as
             * a value means its referent: `(*this->r)`. */
            if (!n->ref_as_ptr && n->str2 != NULL && g_fix_ref_any_members) {
                AstNode *cls = resolve_expr_class(n->a, g_fix_ref_class, locals);
                AstNode *owner = NULL;
                AstNode *member = cls ? find_member_in_hierarchy(cls, n->str2, &owner) : NULL;
                if (member != NULL && member->kind == AST_VAR_DECL && member->is_ref_member) {
                    AstNode *deref = ast_new(AST_UNOP, n->line);
                    deref->str1 = strdup("deref");
                    deref->a = n;
                    *slot = deref;
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
        case AST_SWITCH:
            /* A switch body is a flat statement list with case/default
             * labels mixed in -- walked exactly like a block's. (Missing
             * from this walker until 20261001: calls, this->, references,
             * new/delete, constructors and casts inside case bodies all
             * went unlowered.) */
            fix_reference_access_expr(&n->a, *locals);
            for (int i = 0; i < n->list.count; i++)
                fix_reference_access_stmt(&n->list.items[i], locals);
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
                    g_fix_ref_class = n;
                    fix_references_in_method(layout->methods.items[j]);
                }
                g_fix_ref_class = NULL;
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
        case AST_SWITCH:
            /* A switch body is a flat statement list with case/default
             * labels mixed in -- walked exactly like a block's. (Missing
             * from this walker until 20261001: calls, this->, references,
             * new/delete, constructors and casts inside case bodies all
             * went unlowered.) */
            new_delete_rewrite_expr(&n->a, class_decl, *locals);
            for (int i = 0; i < n->list.count; i++)
                new_delete_rewrite_stmt(&n->list.items[i], class_decl, locals);
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
 *   - An object declared in a for-loop's own init clause
 *     (`for (Player p; ...)`) is moved into a block wrapping the loop
 *     first (inject_ctor_calls_stmt's AST_FOR case, 20261009), so it is
 *     handled exactly like a block statement.
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
        case AST_SWITCH:
            /* A switch body is a flat statement list with case/default
             * labels mixed in -- walked exactly like a block's. (Missing
             * from this walker until 20261001: calls, this->, references,
             * new/delete, constructors and casts inside case bodies all
             * went unlowered.) */
            for (int i = 0; i < s->list.count; i++)
                inject_ctor_calls_stmt(&s->list.items[i], class_decl, locals, arr_ctor_counter);
            break;
        case AST_LABEL:
            inject_ctor_calls_stmt(&s->a, class_decl, locals, arr_ctor_counter);
            break;
        case AST_WHILE:
            inject_ctor_calls_stmt(&s->b, class_decl, locals, arr_ctor_counter);
            break;
        case AST_FOR:
            /* `for (Iter it; it.ok(); ++it)` (or `for (Iter it(v); ...)`):
             * an OBJECT declared in the init clause. Its constructor call
             * has to come between the declaration and the loop, so the
             * declaration moves into a block around the loop -- the same
             * scope C++ gives it -- where the block-level injection
             * constructs it, and phase 9 destroys it after the loop.
             * (Added 20261009; scalars and pointers stay where they are.) */
            if (s->a != NULL && s->a->kind == AST_VAR_DECL && s->a->type != NULL &&
                resolve_typedef_chain(s->a->type)->kind != AST_POINTER_TYPE &&
                resolve_typedef_chain(s->a->type)->kind != AST_REFERENCE_TYPE &&
                type_to_class(s->a->type) != NULL) {
                AstNode *block = ast_new(AST_BLOCK, s->line);
                block->file = s->file;
                ast_list_append(&block->list, s->a);
                s->a = NULL;
                ast_list_append(&block->list, s);
                *slot = block;
                inject_ctor_calls_block(block, class_decl, locals, arr_ctor_counter);
                break;
            }
            /* Otherwise not recursing into s->a: a scalar init needs no
             * constructor (see this phase's own doc comment above). */
            inject_ctor_calls_stmt(&s->d, class_decl, locals, arr_ctor_counter);
            break;
        default:
            break;
    }
}

/* ---- phase 7b: constructing file-scope objects ---------------------------
 *
 * `Random g_rng;` or `std::vector<Enemy> g_enemies;` at file scope: C has
 * nowhere to run a constructor for a global, and a global's memory was
 * observed NOT to start out zeroed under the emulator (a file-scope
 * vector's data pointer was non-null before anything touched it), so
 * such an object started life as whatever was in RAM. C++ constructs
 * every global before main() begins; this
 * does the same by running the block-level injection above over the
 * file-scope declarations and moving the calls it produces to the top
 * of main(), in declaration order.
 *
 * Covers what the local case covers: a default constructor, constructor
 * arguments (`Enemy g_boss(1, 2);`), arrays, and vtable-only classes.
 * Top-level declarations only (not ones inside a namespace), and only
 * when this file defines main(). Destructors of globals are not run:
 * the program ends when main() does.
 */
static void construct_globals_in_main(AstNode *program) {
    AstNode *main_func = NULL;
    AstNode *scratch = ast_new(AST_BLOCK, 0);
    for (int i = 0; i < program->list.count; i++) {
        AstNode *n = program->list.items[i];
        if (n == NULL) continue;
        if (n->kind == AST_FUNC_DEF && n->b == NULL && n->str1 != NULL &&
            strcmp(n->str1, "main") == 0) main_func = n;
        if (n->kind == AST_VAR_DECL && n->type != NULL) ast_list_append(&scratch->list, n);
    }
    if (main_func == NULL || main_func->a == NULL || main_func->a->kind != AST_BLOCK) return;

    LocalVarType *locals = NULL;
    int counter = 1000; /* index-variable numbers well clear of main()'s own */
    inject_ctor_calls_block(scratch, NULL, &locals, &counter);

    AstList body = ast_list_new();
    int added = 0;
    for (int i = 0; i < scratch->list.count; i++) {
        AstNode *st = scratch->list.items[i];
        if (st == NULL || st->kind == AST_VAR_DECL) continue; /* the declaration itself stays at file scope */
        ast_list_append(&body, st);
        added++;
    }
    if (added == 0) return;
    for (int i = 0; i < main_func->a->list.count; i++) ast_list_append(&body, main_func->a->list.items[i]);
    main_func->a->list = body;
    lower_note(main_func->line, "main() now begins by constructing %d file-scope object(s)", added);
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
/* The class of a one-dimensional member array's elements
 * (`Counter items[3];`), or NULL. */
static AstNode *member_array_elem_class(const AstNode *type) {
    while (type != NULL && type->kind == AST_CONST_TYPE) type = type->a;
    if (type == NULL || type->kind != AST_ARRAY_TYPE) return NULL;
    const AstNode *elem = type->a;
    while (elem != NULL && elem->kind == AST_CONST_TYPE) elem = elem->a;
    if (elem == NULL || (elem->kind != AST_IDENT && elem->kind != AST_QUALIFIED_ID)) return NULL;
    return type_to_class(elem);
}

/* `this->member`, or `this->member[index_name]` for an array member. */
static AstNode *member_object_ref(const AstNode *field, const char *index_name, int line) {
    AstNode *ref = ast_new(AST_MEMBER, line);
    ref->str1 = strdup("->");
    ref->str2 = strdup(field->str1);
    ref->a = ast_ident("this", line);
    if (index_name == NULL) return ref;
    AstNode *sub = ast_new(AST_SUBSCRIPT, line);
    sub->a = ref;
    sub->b = ast_ident(index_name, line);
    return sub;
}

/* The array's length as an expression: what the source wrote if it was
 * not a plain literal (a named constant), else the number. */
static AstNode *array_length_expr(const AstNode *array_type, int line) {
    while (array_type != NULL && array_type->kind == AST_CONST_TYPE) array_type = array_type->a;
    if (array_type->b != NULL) return array_type->b;
    AstNode *lit = ast_new(AST_INT_LIT, line);
    lit->ival = array_type->ival;
    return lit;
}

/* Wraps `body_stmt` (which uses `index_name`) in a loop over a member
 * array: forwards for construction, backwards for destruction. */
static AstNode *member_array_loop(const AstNode *array_type, const char *index_name,
                                  AstNode *body_stmt, int backwards, int line) {
    AstNode *idx_decl = ast_new(AST_VAR_DECL, line);
    idx_decl->str1 = strdup(index_name);
    idx_decl->type = ast_ident("int", line);
    AstNode *cond = ast_new(AST_BINOP, line);
    cond->a = ast_ident(index_name, line);
    AstNode *step = ast_new(AST_UNOP, line);
    step->a = ast_ident(index_name, line);
    AstNode *zero = ast_new(AST_INT_LIT, line);
    zero->ival = 0;
    if (!backwards) {
        idx_decl->a = zero;
        cond->str1 = strdup("<");
        cond->b = array_length_expr(array_type, line);
        step->str1 = strdup("post++");
    } else {
        AstNode *one = ast_new(AST_INT_LIT, line);
        one->ival = 1;
        AstNode *last = ast_new(AST_BINOP, line);
        last->str1 = strdup("-");
        last->a = array_length_expr(array_type, line);
        last->b = one;
        idx_decl->a = last;
        cond->str1 = strdup(">=");
        cond->b = zero;
        step->str1 = strdup("post--");
    }
    AstNode *body = ast_new(AST_BLOCK, line);
    ast_list_append(&body->list, body_stmt);
    AstNode *loop = ast_new(AST_FOR, line);
    loop->a = idx_decl;
    loop->b = cond;
    loop->c = step;
    loop->d = body;
    return loop;
}

static int g_member_array_loop_counter = 0;

static AstNode *build_object_ctor_stmt(AstNode *member_ref, AstNode *var_class, int line);

/* A by-value member, or (new) a member ARRAY of class objects, which
 * gets one constructor call per element:
 *   for (int __v32_mctor_iN = 0; __v32_mctor_iN < LEN; __v32_mctor_iN++)
 *       Counter__Counter__void(&this->items[__v32_mctor_iN]);
 * Member arrays used to be skipped outright, leaving every element
 * unconstructed (no constructor body run, no vtable pointer set). */
static AstNode *build_member_ctor_stmt(AstNode *field, int line) {
    AstNode *elem_class = member_array_elem_class(field->type);
    if (elem_class != NULL) {
        char idx[64];
        snprintf(idx, sizeof idx, "__v32_mctor_i%d", g_member_array_loop_counter);
        AstNode *one = build_object_ctor_stmt(member_object_ref(field, idx, line), elem_class, line);
        if (one == NULL) return NULL;
        g_member_array_loop_counter++;
        return member_array_loop(field->type, idx, one, 0, line);
    }
    return build_object_ctor_stmt(member_object_ref(field, NULL, line), type_to_class(field->type), line);
}

static AstNode *build_object_ctor_stmt(AstNode *member_ref, AstNode *var_class, int line) {
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
                        AstNode *var_class = (field->type->kind == AST_ARRAY_TYPE)
                                           ? member_array_elem_class(field->type)
                                           : type_to_class(field->type);
                        if (var_class == NULL) continue;         /* not a class */
                        if (var_class == n) continue;            /* self-recursion */
                        if (member_is_explicitly_initialized(m, field->str1)) {
                            /* `: mVel(vx, vy)` on a class-typed member:
                             * call the constructor sema chose (ival 2,
                             * see resolve_member_init_list) on
                             * &this->member, where the default
                             * construction would have gone. */
                            AstNode *entry = NULL;
                            for (int k = 0; k < m->c->list.count; k++) {
                                if (strcmp(m->c->list.items[k]->str1, field->str1) == 0) entry = m->c->list.items[k];
                            }
                            CallResolution *ecr = (entry != NULL && entry->ival == 2)
                                                ? (CallResolution *)entry->sema_info : NULL;
                            if (ecr != NULL && ecr->resolved_target != NULL &&
                                field->type->kind != AST_ARRAY_TYPE) {
                                AstNode *ctor = ecr->resolved_target;
                                FuncSemaInfo *cinfo = (FuncSemaInfo *)ctor->sema_info;
                                AstList args = entry->list;
                                fill_default_args(&args, ctor, 1);
                                AstNode *addr = ast_new(AST_UNOP, m->line);
                                addr->str1 = strdup("addr");
                                addr->a = member_object_ref(field, NULL, m->line);
                                AstNode *call = ast_new(AST_CALL, m->line);
                                call->a = ast_ident(cinfo != NULL ? cinfo->mangled_name : ctor->str1, m->line);
                                ast_list_append(&call->list, addr);
                                for (int k = 0; k < args.count; k++) ast_list_append(&call->list, args.items[k]);
                                AstNode *st = ast_new(AST_EXPR_STMT, m->line);
                                st->a = call;
                                ast_list_append(&injected, st);
                            }
                            continue;
                        }
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
                        AstNode *member_decl = layout->data_members.items[d];
                        if (member_decl->type != NULL && member_decl->type->kind == AST_REFERENCE_TYPE) {
                            /* A reference member (`int &r;` with `: r(x)`)
                             * stores the ADDRESS of what it is bound to:
                             * `this->r = &x;`. The member itself is wanted
                             * as the pointer here, not dereferenced. */
                            field_ref->ref_as_ptr = 1;
                            AstNode *addr = ast_new(AST_UNOP, m->line);
                            addr->str1 = strdup("addr");
                            addr->a = entry->list.items[0];
                            assign->b = addr;
                        } else
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
                            if (m->c->list.items[k]->sema_info != NULL && m->c->list.items[k]->ival != 2) {
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
    int array_len; /* > 0: the local is a one-dimensional array of that
                       many objects, each destroyed, last element first */
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

    int line = dl->dtor->line;
    static int arr_dtor_counter = 0;
    char idx_name[64];
    snprintf(idx_name, sizeof(idx_name), "__v32_dtor_arr_i%d", arr_dtor_counter);

    AstNode *object = ast_ident(dl->var_name, line);
    if (dl->array_len > 0) {
        AstNode *subscript = ast_new(AST_SUBSCRIPT, line);
        subscript->a = object;
        subscript->b = ast_ident(idx_name, line);
        object = subscript;
    }
    AstNode *addr = ast_new(AST_UNOP, line);
    addr->str1 = strdup("addr");
    addr->a = object;

    AstNode *call = ast_new(AST_CALL, line);
    call->a = ast_ident(dtor_mangled, line);
    ast_list_append(&call->list, addr);

    AstNode *expr_stmt = ast_new(AST_EXPR_STMT, line);
    expr_stmt->a = call;
    if (dl->array_len <= 0) return expr_stmt;

    /* `Derived arr[3];` leaving scope: the mirror of build_array_ctor_loop,
     * counting down --
     *   for (int i = N - 1; i >= 0; i--) Derived__dtor(&arr[i]);
     * Arrays were constructed element by element but never destroyed. */
    arr_dtor_counter++;
    AstNode *idx_decl = ast_new(AST_VAR_DECL, line);
    idx_decl->str1 = strdup(idx_name);
    idx_decl->type = ast_ident("int", line);
    idx_decl->a = ast_new(AST_INT_LIT, line);
    idx_decl->a->ival = dl->array_len - 1;

    AstNode *cond = ast_new(AST_BINOP, line);
    cond->str1 = strdup(">=");
    cond->a = ast_ident(idx_name, line);
    cond->b = ast_new(AST_INT_LIT, line);
    cond->b->ival = 0;

    AstNode *step = ast_new(AST_UNOP, line);
    step->str1 = strdup("post--");
    step->a = ast_ident(idx_name, line);

    AstNode *body = ast_new(AST_BLOCK, line);
    ast_list_append(&body->list, expr_stmt);

    AstNode *loop = ast_new(AST_FOR, line);
    loop->a = idx_decl;
    loop->b = cond;
    loop->c = step;
    loop->d = body;
    return loop;
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

        int array_len = 0;
        AstNode *obj_type = (stmt->kind == AST_VAR_DECL) ? stmt->type : NULL;
        if (obj_type != NULL && obj_type->kind == AST_ARRAY_TYPE && obj_type->ival > 0 &&
            obj_type->a != NULL) {
            array_len = obj_type->ival;      /* one dimension only */
            obj_type = obj_type->a;
        }
        if (obj_type != NULL &&
            (obj_type->kind == AST_IDENT || obj_type->kind == AST_QUALIFIED_ID)) {
            AstNode *var_class = type_to_class(obj_type);
            if (var_class != NULL) {
                AstNode *dtor = find_destructor_with_body(var_class);
                if (dtor != NULL) {
                    DestructibleLocal *dl = malloc(sizeof(DestructibleLocal));
                    dl->var_name = stmt->str1;
                    dl->dtor = dtor;
                    dl->array_len = array_len;
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
static const AstNode *g_cast_return_type = NULL; /* the enclosing function's return type */

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
        case AST_SWITCH:
            /* A switch body is a flat statement list with case/default
             * labels mixed in -- walked exactly like a block's. (Missing
             * from this walker until 20261001: calls, this->, references,
             * new/delete, constructors and casts inside case bodies all
             * went unlowered.) */
            for (int i = 0; i < n->list.count; i++)
                insert_pointer_cast_stmt(&n->list.items[i], class_decl, locals);
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
        case AST_RETURN: {
            /* `Shape *self() { return this; }` inside Square: the same
             * derived-to-base pointer conversion the declaration case
             * above spells out, for a returned pointer. */
            const AstNode *rt = g_cast_return_type;
            if (n->a == NULL || rt == NULL || rt->kind != AST_POINTER_TYPE) break;
            AstNode *declared_class = type_to_class(rt);
            AstNode *value_type = infer_expr_type(n->a, class_decl, *locals);
            if (declared_class == NULL || value_type == NULL || value_type->kind != AST_POINTER_TYPE) break;
            AstNode *value_class = type_to_class(value_type);
            if (value_class == NULL || value_class == declared_class) break;
            AstNode *cast = ast_new(AST_CAST, n->a->line);
            cast->type = ast_wrap_pointer(ast_ident(declared_class->str1, n->a->line), n->a->line);
            cast->a = n->a;
            n->a = cast;
            break;
        }
        default:
            break;
    }
}

static void insert_pointer_cast_in_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return;
    LocalVarType *locals = seed_locals_from_params(method);
    g_cast_return_type = method->type;
    insert_pointer_cast_stmt(&method->a, class_decl, &locals);
    g_cast_return_type = NULL;
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
 * The Vircon32 C lexer doesn't accept '?' at all, so no `cond ? a : b`
 * may reach Vircon32-mode output -- and Vircon32 C has no comma operator
 * either, so `a, b` is lowered here the same way (a becomes a statement
 * of its own, run first; the expression is b). This phase rewrites every ternary in
 * every function body into if/else, and lower_check_no_ternaries (run
 * right after it) guarantees nothing slipped through: a leftover ternary
 * is folded if it's an integer constant (a global or enum initializer)
 * or reported as an error -- never printed.
 *
 * The rewrite keeps C++'s evaluation rules, not just its values:
 *   - a ternary's condition is evaluated first, and ONLY the chosen branch
 *     is evaluated afterwards -- branches are rewritten inside their own
 *     if/else blocks, so `p ? p->x : 0` and `a ? b : (c ? f() : 0)` never
 *     touch the untaken side;
 *   - a ternary on the right of && / || is evaluated only when the left
 *     side doesn't already decide the result (`p && (p->x ? 1 : 0)`);
 *   - a ternary in a loop condition (while, do-while, for) or a for
 *     increment is re-evaluated on EVERY iteration: the loop becomes
 *     `while (1)` with the condition computed at the top of each pass (a
 *     first-pass flag runs the increment / skips the do-while test the
 *     first time), so break and continue keep their meaning;
 *   - a brace-less body (`if (a) f(b ? 1 : 2);`), an else-if condition, a
 *     labeled statement, a for-init clause and a switch's case bodies all
 *     get a block to hold the hoisted statements -- in a switch, always a
 *     NESTED block, because Vircon32 C rejects any declaration in a switch
 *     body after a case label ("variables cannot be declared in a switch
 *     after case/default are used", confirmed against the real compiler).
 *
 * Three shapes are rewritten directly with no temporary, since they read
 * best that way: `T v = c ? a : b;`, `v = c ? a : b;` and
 * `return c ? a : b;`. Everything else goes into a fresh
 * __v32_tern_tmpN declared just before the statement (int when no
 * branch's type can be inferred, or an enum meets a non-enum; float when
 * one branch is float).
 *
 * Runs only when g_target == TARGET_VIRCON32 -- standard C keeps `?:`.
 */

typedef struct TernCtx {
    AstNode *class_decl;
    LocalVarType *locals;
    int tmp_counter;
} TernCtx;

static int is_comma(const AstNode *n) {
    return n != NULL && n->kind == AST_BINOP && n->str1 != NULL && strcmp(n->str1, ",") == 0;
}

/* Does this subtree contain something Vircon32 C lacks -- a ternary or a
 * comma operator? Walks the expression/statement slots (a..d, list),
 * never `type`. */
static int has_ternary(const AstNode *n) {
    if (n == NULL) return 0;
    if (n->kind == AST_TERNARY || is_comma(n)) return 1;
    if (has_ternary(n->a) || has_ternary(n->b) || has_ternary(n->c) || has_ternary(n->d))
        return 1;
    for (int i = 0; i < n->list.count; i++)
        if (has_ternary(n->list.items[i])) return 1;
    return 0;
}

static char *tern_tmp_name(TernCtx *cx) {
    char name[40];
    snprintf(name, sizeof(name), "__v32_tern_tmp%d", cx->tmp_counter++);
    return strdup(name);
}

static void tern_add_local(TernCtx *cx, char *name, AstNode *type) {
    LocalVarType *lv = calloc(1, sizeof(LocalVarType));
    lv->name = name;
    lv->type = type;
    lv->next = cx->locals;
    cx->locals = lv;
}

static AstNode *tern_block(int line) {
    return ast_new(AST_BLOCK, line);
}

static AstNode *tern_block_of(AstNode *stmt, int line) {
    AstNode *b = tern_block(line);
    ast_list_append(&b->list, stmt);
    return b;
}

static AstNode *tern_if(AstNode *cond, AstNode *then_block, AstNode *else_block, int line) {
    AstNode *n = ast_new(AST_IF, line);
    n->a = cond;
    n->b = then_block;
    n->c = else_block;
    return n;
}

static AstNode *tern_assign_stmt(const char *name, AstNode *rhs, int line) {
    AstNode *as = ast_new(AST_ASSIGN, line);
    as->str1 = strdup("=");
    as->a = ast_ident(name, line);
    as->b = rhs;
    AstNode *st = ast_new(AST_EXPR_STMT, line);
    st->a = as;
    return st;
}

static AstNode *tern_int(int v, int line) {
    AstNode *n = ast_new(AST_INT_LIT, line);
    n->ival = v;
    return n;
}

/* `if (cond) { } else { break; }` -- the loop-exit test. Spelled without
 * a `!` so a pointer-valued condition needs no conversion. */
static AstNode *tern_break_unless(AstNode *cond, int line) {
    return tern_if(cond, tern_block(line), tern_block_of(ast_new(AST_BREAK, line), line), line);
}

/* Declares a fresh temporary (in `out`) and returns its name. */
static char *tern_declare(TernCtx *cx, AstList *out, AstNode *type, AstNode *init, int line) {
    /* The temporary is assigned to after it is declared, so it cannot be
     * const, whatever the expression it stands in for is: a ternary whose
     * first branch read a `const int` table gave `const int tmp;` and
     * then `tmp = ...`, which Vircon32 C rejects. Only the top-level
     * const goes; `const char *` (pointer to const) stays as it is. */
    while (type != NULL && type->kind == AST_CONST_TYPE && init == NULL) type = type->a;
    char *name = tern_tmp_name(cx);
    AstNode *decl = ast_new(AST_VAR_DECL, line);
    decl->str1 = name;
    decl->type = type;
    decl->a = init;
    ast_list_append(out, decl);
    tern_add_local(cx, name, type);
    return name;
}

static int is_float_type(const AstNode *t) {
    return t != NULL && t->kind == AST_IDENT && t->str1 != NULL && strcmp(t->str1, "float") == 0;
}

static int is_int_like_type(const AstNode *t) {
    return t != NULL && t->kind == AST_IDENT && t->str1 != NULL &&
           (strcmp(t->str1, "int") == 0 || strcmp(t->str1, "char") == 0 ||
            strcmp(t->str1, "bool") == 0);
}

/* The temporary's type: C++'s usual arithmetic conversion where it
 * matters here (int with float gives float, so `c ? 1 : 2.5` can't
 * truncate), else whichever branch's type is known, else int. */
static AstNode *ternary_type(AstNode *t, TernCtx *cx) {
    AstNode *tb = infer_expr_type(t->b, cx->class_decl, cx->locals);
    AstNode *tc = infer_expr_type(t->c, cx->class_decl, cx->locals);
    /* An enum meeting anything but the SAME enum converts to int in C++
     * (`on ? v32::BlendAlpha : 0`), and Vircon32 C refuses an int stored
     * into an enum-typed temporary -- so the temporary is an int. */
    int eb = sema_is_enum_type(tb), ec = sema_is_enum_type(tc);
    if ((eb || ec) && !(eb && ec && tb->kind == AST_IDENT && tc->kind == AST_IDENT &&
                        strcmp(tb->str1, tc->str1) == 0))
        return ast_ident("int", t->line);
    if ((is_float_type(tb) && is_int_like_type(tc)) || (is_int_like_type(tb) && is_float_type(tc)))
        return ast_ident("float", t->line);
    if (tb != NULL) return tb;
    if (tc != NULL) return tc;
    return ast_ident("int", t->line);
}

static void tern_rewrite_stmt(AstNode *s, TernCtx *cx, AstList *out);
static void tern_rewrite_list(AstList *list, TernCtx *cx, int in_switch);

/* Rewrites one statement into a fresh block's list. */
static AstNode *tern_block_rewriting(AstNode *stmt, TernCtx *cx, int line) {
    AstNode *b = tern_block(line);
    tern_rewrite_stmt(stmt, cx, &b->list);
    return b;
}

/* Moves every ternary out of the expression in *slot: the statements
 * that compute them are appended to `pre` (to run just before the
 * statement owning the expression) and each ternary is replaced by its
 * temporary. Evaluation order and laziness are kept -- see the phase
 * comment above. */
static void tern_lower_expr(AstNode **slot, TernCtx *cx, AstList *pre) {
    AstNode *n = *slot;
    if (n == NULL || !has_ternary(n)) return;
    int line = n->line;

    if (n->kind == AST_TERNARY) {
        tern_lower_expr(&n->a, cx, pre);         /* the condition runs first */
        AstNode *type = ternary_type(n, cx);
        char *tmp = tern_declare(cx, pre, type, NULL, line);
        lower_note(line, "hoisted a `?:` into %s plus an if/else (Vircon32 C has no ternary operator)", tmp);
        AstNode *then_b = tern_block_rewriting(tern_assign_stmt(tmp, n->b, line), cx, line);
        AstNode *else_b = tern_block_rewriting(tern_assign_stmt(tmp, n->c, line), cx, line);
        ast_list_append(pre, tern_if(n->a, then_b, else_b, line));
        *slot = ast_ident(tmp, line);
        return;
    }

    if (is_comma(n)) {
        /* `a, b`: a runs first, as a statement of its own (it may hold
         * ternaries or commas itself); the expression is then b. */
        AstNode *first = ast_new(AST_EXPR_STMT, line);
        first->a = n->a;
        tern_rewrite_stmt(first, cx, pre);
        *slot = n->b;
        tern_lower_expr(slot, cx, pre);
        return;
    }

    if (n->kind == AST_BINOP && n->str1 != NULL &&
        (strcmp(n->str1, "&&") == 0 || strcmp(n->str1, "||") == 0) && has_ternary(n->b)) {
        /* The right side may only run when the left doesn't decide:
         *   t = 0; if (a) { if (b) { t = 1; } }                 (&&)
         *   t = 0; if (a) { t = 1; } else { if (b) { t = 1; } } (||) */
        int is_and = (n->str1[0] == '&');
        tern_lower_expr(&n->a, cx, pre);
        char *tmp = tern_declare(cx, pre, ast_ident("int", line), tern_int(0, line), line);
        AstNode *inner = tern_if(n->b, tern_block_of(tern_assign_stmt(tmp, tern_int(1, line), line), line),
                                 NULL, line);
        AstNode *inner_b = tern_block_rewriting(inner, cx, line);
        AstNode *outer = is_and
            ? tern_if(n->a, inner_b, NULL, line)
            : tern_if(n->a, tern_block_of(tern_assign_stmt(tmp, tern_int(1, line), line), line),
                      inner_b, line);
        ast_list_append(pre, outer);
        *slot = ast_ident(tmp, line);
        return;
    }

    /* Anything else: its operands, in order. */
    tern_lower_expr(&n->a, cx, pre);
    tern_lower_expr(&n->b, cx, pre);
    tern_lower_expr(&n->c, cx, pre);
    tern_lower_expr(&n->d, cx, pre);
    for (int i = 0; i < n->list.count; i++)
        tern_lower_expr(&n->list.items[i], cx, pre);
}

/* A statement slot that isn't a list (an if/loop body, else branch,
 * labeled statement): rewritten in place, becoming a block when the
 * rewrite produced more than one statement. */
static void tern_rewrite_child(AstNode **slot, TernCtx *cx) {
    AstNode *s = *slot;
    if (s == NULL) return;
    if (s->kind == AST_BLOCK) {
        tern_rewrite_list(&s->list, cx, 0);
        return;
    }
    if (!has_ternary(s)) return;
    AstList out = ast_list_new();
    tern_rewrite_stmt(s, cx, &out);
    if (out.count == 1) {
        *slot = out.items[0];
    } else {
        AstNode *b = tern_block(s->line);
        b->list = out;
        *slot = b;
    }
}

static AstNode *tern_as_block(AstNode *s, int line) {
    if (s == NULL) return tern_block(line);
    return s->kind == AST_BLOCK ? s : tern_block_of(s, line);
}

/* Rewrites statement `s`, appending it -- preceded by whatever its
 * ternaries needed -- to `out`. */
static void tern_rewrite_stmt(AstNode *s, TernCtx *cx, AstList *out) {
    int line = s->line;
    switch (s->kind) {
        case AST_BLOCK:
            tern_rewrite_list(&s->list, cx, 0);
            break;

        case AST_VAR_DECL:
            if (s->a != NULL && s->a->kind == AST_TERNARY) {
                /* direct: `T v; if (c) { v = a; } else { v = b; }` */
                AstNode *t = s->a;
                tern_lower_expr(&t->a, cx, out);
                s->a = NULL;
                ast_list_append(out, s);
                tern_add_local(cx, s->str1, s->type);
                lower_note(line, "rewrote `%s = cond ? a : b` (declaration) into an if/else", s->str1);
                AstNode *then_b = tern_block_rewriting(tern_assign_stmt(s->str1, t->b, line), cx, line);
                AstNode *else_b = tern_block_rewriting(tern_assign_stmt(s->str1, t->c, line), cx, line);
                ast_list_append(out, tern_if(t->a, then_b, else_b, line));
                return;
            }
            tern_lower_expr(&s->a, cx, out);
            ast_list_append(out, s);
            tern_add_local(cx, s->str1, s->type);
            return;

        case AST_EXPR_STMT:
            if (s->a != NULL && s->a->kind == AST_ASSIGN && s->a->str1 != NULL &&
                strcmp(s->a->str1, "=") == 0 && s->a->a != NULL && s->a->a->kind == AST_IDENT &&
                s->a->b != NULL && s->a->b->kind == AST_TERNARY) {
                /* direct: `if (c) { v = a; } else { v = b; }` */
                AstNode *t = s->a->b;
                const char *name = s->a->a->str1;
                tern_lower_expr(&t->a, cx, out);
                AstNode *then_b = tern_block_rewriting(tern_assign_stmt(name, t->b, line), cx, line);
                AstNode *else_b = tern_block_rewriting(tern_assign_stmt(name, t->c, line), cx, line);
                ast_list_append(out, tern_if(t->a, then_b, else_b, line));
                return;
            }
            tern_lower_expr(&s->a, cx, out);
            break;

        case AST_RETURN:
            if (s->a != NULL && s->a->kind == AST_TERNARY) {
                /* direct: `if (c) { return a; } else { return b; }` */
                AstNode *t = s->a;
                tern_lower_expr(&t->a, cx, out);
                AstNode *ret_b = ast_new(AST_RETURN, line);
                ret_b->a = t->b;
                AstNode *ret_c = ast_new(AST_RETURN, line);
                ret_c->a = t->c;
                ast_list_append(out, tern_if(t->a, tern_block_rewriting(ret_b, cx, line),
                                             tern_block_rewriting(ret_c, cx, line), line));
                return;
            }
            tern_lower_expr(&s->a, cx, out);
            break;

        case AST_IF:
            tern_lower_expr(&s->a, cx, out);
            tern_rewrite_child(&s->b, cx);
            tern_rewrite_child(&s->c, cx);
            break;

        case AST_WHILE:
            if (has_ternary(s->a)) {
                AstNode *body = tern_as_block(s->b, line);
                tern_rewrite_list(&body->list, cx, 0);
                AstNode *loop = tern_block(line);
                if (s->ival == 1) {
                    /* do { body } while (c);  ->
                     *   first = 1;
                     *   while (1) { if (first) { } else { <c>; exit unless c }
                     *               first = 0; body } */
                    char *first = tern_declare(cx, out, ast_ident("int", line), tern_int(1, line), line);
                    AstNode *test = tern_block(line);
                    AstNode *cond = s->a;
                    tern_lower_expr(&cond, cx, &test->list);
                    ast_list_append(&test->list, tern_break_unless(cond, line));
                    ast_list_append(&loop->list, tern_if(ast_ident(first, line), tern_block(line), test, line));
                    ast_list_append(&loop->list, tern_assign_stmt(first, tern_int(0, line), line));
                } else {
                    /* while (c) body  ->  while (1) { <c>; exit unless c; body } */
                    AstNode *cond = s->a;
                    tern_lower_expr(&cond, cx, &loop->list);
                    ast_list_append(&loop->list, tern_break_unless(cond, line));
                }
                ast_list_append(&loop->list, body);
                lower_note(line, "loop condition contains `?:`: rewritten as while (1) with the test re-evaluated each pass");
                s->ival = 0;
                s->a = tern_int(1, line);
                s->b = loop;
                break;
            }
            tern_rewrite_child(&s->b, cx);
            break;

        case AST_FOR:
            if (has_ternary(s->a) || has_ternary(s->b) || has_ternary(s->c)) {
                /* The init clause moves into a wrapping block (same scope
                 * as before); a ternary condition or increment turns the
                 * loop into
                 *   first = 1;
                 *   while (1) { if (first) { } else { incr; } first = 0;
                 *               <c>; exit unless c; body }
                 * so `continue` still runs the increment, then the test. */
                AstNode *wrapper = tern_block(line);
                if (s->a != NULL) tern_rewrite_stmt(s->a, cx, &wrapper->list);
                s->a = NULL;
                if (has_ternary(s->b) || has_ternary(s->c)) {
                    AstNode *loop = tern_block(line);
                    if (s->c != NULL) {
                        char *first = tern_declare(cx, &wrapper->list, ast_ident("int", line),
                                                   tern_int(1, line), line);
                        AstNode *incr = ast_new(AST_EXPR_STMT, line);
                        incr->a = s->c;
                        ast_list_append(&loop->list, tern_if(ast_ident(first, line), tern_block(line),
                                                             tern_block_rewriting(incr, cx, line), line));
                        ast_list_append(&loop->list, tern_assign_stmt(first, tern_int(0, line), line));
                    }
                    if (s->b != NULL) {
                        AstNode *cond = s->b;
                        tern_lower_expr(&cond, cx, &loop->list);
                        ast_list_append(&loop->list, tern_break_unless(cond, line));
                    }
                    AstNode *body = tern_as_block(s->d, line);
                    tern_rewrite_list(&body->list, cx, 0);
                    ast_list_append(&loop->list, body);
                    AstNode *w = ast_new(AST_WHILE, line);
                    w->a = tern_int(1, line);
                    w->b = loop;
                    ast_list_append(&wrapper->list, w);
                    lower_note(line, "for-loop condition/increment contains `?:`: rewritten as while (1)");
                } else {
                    tern_rewrite_child(&s->d, cx);
                    ast_list_append(&wrapper->list, s);
                }
                ast_list_append(out, wrapper);
                return;
            }
            tern_rewrite_child(&s->d, cx);
            break;

        case AST_SWITCH:
            tern_lower_expr(&s->a, cx, out);
            tern_rewrite_list(&s->list, cx, 1);
            break;

        case AST_LABEL: {
            /* The hoisted statements must come AFTER the label, or a goto
             * to it would skip them. */
            AstList inner = ast_list_new();
            if (s->a != NULL) tern_rewrite_stmt(s->a, cx, &inner);
            if (inner.count == 1) {
                s->a = inner.items[0];
            } else if (inner.count > 1) {
                AstNode *b = tern_block(line);
                b->list = inner;
                s->a = b;
            }
            break;
        }

        default:
            /* case labels, break/continue/goto, asm: nothing to rewrite
             * (a ternary case VALUE is folded by lower_check_no_ternaries) */
            break;
    }
    ast_list_append(out, s);
}

static void tern_rewrite_list(AstList *list, TernCtx *cx, int in_switch) {
    AstList result = ast_list_new();
    for (int i = 0; i < list->count; i++) {
        AstNode *stmt = list->items[i];
        if (!has_ternary(stmt)) {
            ast_list_append(&result, stmt);
            if (stmt->kind == AST_VAR_DECL) tern_add_local(cx, stmt->str1, stmt->type);
            continue;
        }
        AstList out = ast_list_new();
        tern_rewrite_stmt(stmt, cx, &out);
        if (in_switch && out.count > 1) {
            /* Vircon32 C: no declarations in a switch body after a case
             * label -- give the hoisted temporaries their own block. */
            AstNode *b = tern_block(stmt->line);
            b->list = out;
            ast_list_append(&result, b);
        } else {
            for (int j = 0; j < out.count; j++) ast_list_append(&result, out.items[j]);
        }
    }
    *list = result;
}

static void rewrite_ternary_in_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return;
    TernCtx cx = { class_decl, seed_locals_from_params(method), 0 };
    tern_rewrite_child(&method->a, &cx);
}

/* The guarantee behind phase 10: after it, no AST_TERNARY may remain
 * anywhere in Vircon32-mode output. One outside a function body (a
 * global's or enum value's initializer, a default argument, a case
 * value) can't be rewritten into statements, but is almost always an
 * integer constant -- folded to its value in place. Anything else is an
 * error naming the line, instead of a '?' the Vircon32 C lexer would
 * die on downstream with no hint of where it came from. Returns the
 * number of errors reported. */
static int check_no_ternaries(AstNode *n) {
    if (n == NULL) return 0;
    int errors = 0;
    if (is_comma(n)) {
        fprintf(stderr, "lowering error at %s:%d: the comma operator can't be lowered "
                "for Vircon32 C outside a function body\n", n->file ? n->file : "?", n->line);
        return 1;
    }
    if (n->kind == AST_TERNARY) {
        int v;
        if (ast_fold_int(n, NULL, &v)) {
            lower_note(n->line, "folded a constant `?:` to %d", v);
            n->kind = AST_INT_LIT;
            n->ival = v;
            n->a = n->b = n->c = NULL;
            return 0;
        }
        fprintf(stderr, "lowering error at %s:%d: this `?:` can't be lowered for "
                "Vircon32 C (which has no ternary operator) -- outside a function "
                "body it must be an integer constant; use if/else instead\n",
                n->file ? n->file : "?", n->line);
        return 1;
    }
    errors += check_no_ternaries(n->a);
    errors += check_no_ternaries(n->b);
    /* A function's `c` is its member-initializer list. By now every entry
     * has been turned into a statement at the top of the body (where any
     * `?:` in it was lowered like the rest); the list itself is a spent
     * copy, never emitted, and still holds the original expression --
     * `: mEnergy(mega ? 6 : 1)` was reported here as a `?:` "outside a
     * function body". */
    if (n->kind != AST_FUNC_DEF) errors += check_no_ternaries(n->c);
    errors += check_no_ternaries(n->d);
    for (int i = 0; i < n->list.count; i++) errors += check_no_ternaries(n->list.items[i]);
    return errors;
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

/* ---- phase 11: Vircon32 C value-compatibility fixes (--target=vircon32 only)
 *
 * Two places where Vircon32 C is stricter than standard C about VALID C,
 * both found by running a plain-C program (demos/c/spyvsspy) through the
 * real compiler and both confirmed one construct at a time against it
 * (docs/VIRCON32_QUIRKS.md has the probe results):
 *
 * 1. Arrays only decay to a pointer in a plain assignment, initializer or
 *    call argument. As an operand they stay arrays and are rejected:
 *        g_actors + MAX_ACTORS      "invalid operands for addition"
 *        it - g_actors              "invalid operands for subtraction"
 *        p == g_actors              "invalid operands for equality comparison"
 *        (int)g_actors              "cannot convert expression type"
 *        *g_actors                  "dereference can only be applied to pointers"
 *    Pointer arithmetic itself is fine (`p + 2`, `q - p`, `p += n`), so the
 *    fix is just to spell the decay out: an array-typed operand of + - and
 *    the comparisons, of a cast, or of unary `*` becomes `&arr[0]`.
 *
 * 2. `const` sticks to a VALUE that is merely read. Copying a const scalar
 *    is legal C (the copy isn't const), but Vircon32 reports "cannot assign
 *    const int to int: discards const qualifier" for
 *        n = c;      f( c );      return c;      n = -c;      n = cp->x;
 *    -- though not for an initializer (`int n = c;`) or once the value has
 *    been through a binary operator (`n = c + 1`). A cast to the value's
 *    own unqualified type makes it an rvalue and is accepted, so that is
 *    what gets inserted at exactly those three read positions (assignment
 *    right-hand side, call argument, return value). A const STRUCT can't
 *    be cast by value; it is read through a de-const'ed pointer instead,
 *    `*((S *)&expr)`.
 *    Writes to const objects and genuine qualifier discards on POINTERS
 *    (`S *q = cp;` with `const S *cp`) are untouched -- those are real
 *    errors and stay the downstream compiler's to report.
 *
 * Runs last, after ternary lowering, so the temporaries that phase
 * introduces (`__v32_tern_tmpN = lo;` with a const `lo`) are covered too.
 * This supersedes, without replacing, strip_const_member_read's earlier
 * and narrower member/subscript-only version of fix 2: anything that one
 * already wrapped is an AST_CAST by now and is left alone here.
 */
static const AstNode *v32_strip_const(const AstNode *t) {
    t = resolve_typedef_chain(t);
    while (t != NULL && t->kind == AST_CONST_TYPE) t = resolve_typedef_chain(t->a);
    return t;
}

static int v32_type_is_const(const AstNode *t) {
    t = resolve_typedef_chain(t);
    return t != NULL && t->kind == AST_CONST_TYPE;
}

static int v32_expr_is_array(const AstNode *e, AstNode *cls, LocalVarType *locals) {
    if (e == NULL) return 0;
    if (e->kind != AST_IDENT && e->kind != AST_MEMBER && e->kind != AST_SUBSCRIPT) return 0;
    const AstNode *t = v32_strip_const(infer_expr_type(e, cls, locals));
    return t != NULL && t->kind == AST_ARRAY_TYPE;
}

/* `arr` -> `&arr[0]` */
static void v32_decay_array(AstNode **slot, AstNode *cls, LocalVarType *locals) {
    AstNode *e = *slot;
    if (!v32_expr_is_array(e, cls, locals)) return;
    AstNode *zero = ast_new(AST_INT_LIT, e->line);
    zero->ival = 0;
    AstNode *sub = ast_new(AST_SUBSCRIPT, e->line);
    sub->a = e;
    sub->b = zero;
    AstNode *addr = ast_new(AST_UNOP, e->line);
    addr->str1 = strdup("addr");
    addr->a = sub;
    *slot = addr;
    lower_note(e->line, "spelled out an array-to-pointer decay as &array[0] "
        "-- Vircon32 C only decays an array in a plain assignment, "
        "initializer or argument, not as an operand");
}

/* Is `e` an lvalue whose VALUE is const-qualified -- declared const, or
 * reached through a const object / pointer-to-const? */
static int v32_lvalue_is_const(const AstNode *e, AstNode *cls, LocalVarType *locals) {
    if (e == NULL) return 0;
    switch (e->kind) {
        case AST_IDENT: {
            const AstNode *t = resolve_typedef_chain(infer_expr_type(e, cls, locals));
            if (t == NULL) return 0;
            if (t->kind == AST_CONST_TYPE) return 1;
            return t->kind == AST_ARRAY_TYPE && v32_type_is_const(t->a);
        }
        case AST_MEMBER: {
            if (v32_type_is_const(infer_expr_type(e, cls, locals))) return 1;
            const AstNode *ot = resolve_typedef_chain(infer_expr_type(e->a, cls, locals));
            if (ot == NULL) return 0;
            if (ot->kind == AST_POINTER_TYPE) return v32_type_is_const(ot->a);
            return v32_lvalue_is_const(e->a, cls, locals);
        }
        case AST_SUBSCRIPT: {
            const AstNode *bt = resolve_typedef_chain(infer_expr_type(e->a, cls, locals));
            if (bt == NULL) return 0;
            if (bt->kind == AST_CONST_TYPE) bt = resolve_typedef_chain(bt->a);
            if (bt == NULL) return 0;
            if (bt->kind == AST_POINTER_TYPE) return v32_type_is_const(bt->a);
            if (bt->kind == AST_ARRAY_TYPE)
                return v32_type_is_const(bt->a) || v32_lvalue_is_const(e->a, cls, locals);
            return 0;
        }
        case AST_UNOP:
            if (e->str1 != NULL && strcmp(e->str1, "deref") == 0) {
                const AstNode *pt = v32_strip_const(infer_expr_type(e->a, cls, locals));
                return pt != NULL && pt->kind == AST_POINTER_TYPE && v32_type_is_const(pt->a);
            }
            return 0;
        default:
            return 0;
    }
}

static int v32_is_builtin_scalar(const AstNode *t) {
    return t != NULL && t->kind == AST_IDENT && t->str1 != NULL &&
           (strcmp(t->str1, "int") == 0 || strcmp(t->str1, "float") == 0 ||
            strcmp(t->str1, "bool") == 0 || strcmp(t->str1, "char") == 0);
}

/* A value read at an assignment RHS / argument / return position. */
static void v32_unconst_read(AstNode **slot, AstNode *cls, LocalVarType *locals) {
    AstNode *e = *slot;
    if (e == NULL) return;
    /* `-c` and `~c` keep c's const as far as Vircon32 is concerned. */
    const AstNode *core = e;
    while (core->kind == AST_UNOP && core->str1 != NULL && core->a != NULL &&
           (strcmp(core->str1, "neg") == 0 || strcmp(core->str1, "~") == 0)) {
        core = core->a;
    }
    if (!v32_lvalue_is_const(core, cls, locals)) return;
    AstNode *vt = (AstNode *)v32_strip_const(infer_expr_type(e, cls, locals));
    if (vt == NULL) return;
    if (v32_is_builtin_scalar(vt) ||
        (vt->kind == AST_POINTER_TYPE && vt->a != NULL &&
         (vt->a->kind == AST_IDENT || vt->a->kind == AST_CONST_TYPE || vt->a->kind == AST_POINTER_TYPE)) ||
        sema_is_enum_type(vt)) {
        AstNode *cast = ast_new(AST_CAST, e->line);
        cast->type = vt;
        cast->a = e;
        *slot = cast;
        lower_note(e->line, "inserted a cast around a read of a const value "
            "-- Vircon32 C rejects copying a const-qualified value into a "
            "plain one (\"discards const qualifier\"), which standard C allows");
    } else if (core == e && (vt->kind == AST_IDENT || vt->kind == AST_QUALIFIED_ID) &&
               type_to_class(vt) != NULL) {
        /* const struct, copied by value: *((S *)&expr) */
        AstNode *addr = ast_new(AST_UNOP, e->line);
        addr->str1 = strdup("addr");
        addr->a = e;
        AstNode *cast = ast_new(AST_CAST, e->line);
        cast->type = ast_wrap_pointer(vt, e->line); /* vt itself: it may be qualified (std::string) */
        cast->a = addr;
        AstNode *deref = ast_new(AST_UNOP, e->line);
        deref->str1 = strdup("deref");
        deref->a = cast;
        *slot = deref;
        lower_note(e->line, "read a const struct through a (%s *) cast -- "
            "Vircon32 C rejects copying a const struct into a plain one", type_to_class(vt)->str1);
    }
}

static int v32_op_decays(const char *op) {
    return op != NULL && (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
        strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
        strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0);
}

/* ---- compound assignment to a target with side effects ---------------------
 *
 * Vircon32 C compiles `target op= value` as `target = target op value`
 * with the target written out twice, so a side effect in it happens
 * twice: `*p++ ^= key;` reads through p, stores through p + 1, and leaves
 * p two further on; `rooms[rnd_room()].r_flags |= ISGONE;` picks two
 * different rooms (both confirmed with the real compiler). When the
 * assignment is a statement of its own, the target's address is taken
 * once:
 *
 *     *p++ ^= key;      ->      { char *__v32_target0 = &*p++;
 *                                 *__v32_target0 ^= key; }
 *
 * or, where the target's type is not known, ++ / -- of plain variables are
 * moved out of it: `{ *p ^= key; p++; }`. Anything else (the assignment
 * used as a value inside a larger expression) is left as written, with a
 * warning.
 */
static int v32_is_step(const AstNode *e) {
    return e != NULL && e->kind == AST_UNOP && e->str1 != NULL &&
           (strcmp(e->str1, "post++") == 0 || strcmp(e->str1, "post--") == 0 ||
            strcmp(e->str1, "pre++") == 0 || strcmp(e->str1, "pre--") == 0);
}

static int v32_has_side_effect(const AstNode *e) {
    if (e == NULL) return 0;
    if (v32_is_step(e) || e->kind == AST_ASSIGN || e->kind == AST_CALL) return 1;
    if (v32_has_side_effect(e->a) || v32_has_side_effect(e->b) || v32_has_side_effect(e->c)) return 1;
    for (int i = 0; i < e->list.count; i++)
        if (v32_has_side_effect(e->list.items[i])) return 1;
    return 0;
}

/* Moves every ++ / -- of a plain variable out of *slot into `before` or
 * `after`. Returns 0 if a side effect it cannot move is in there. */
static int v32_extract_steps(AstNode **slot, AstList *before, AstList *after) {
    AstNode *e = *slot;
    if (e == NULL) return 1;
    if (v32_is_step(e)) {
        if (e->a == NULL || e->a->kind != AST_IDENT) return 0;
        AstNode *stmt = ast_new(AST_EXPR_STMT, e->line);
        stmt->a = e;
        ast_list_append(e->str1[1] == 'r' ? before : after, stmt);   /* p-r-e / p-o-st */
        *slot = ast_clone_expr(e->a);
        return 1;
    }
    if (e->kind == AST_ASSIGN || e->kind == AST_CALL) return 0;
    if (!v32_extract_steps(&e->a, before, after) || !v32_extract_steps(&e->b, before, after) ||
        !v32_extract_steps(&e->c, before, after))
        return 0;
    for (int i = 0; i < e->list.count; i++)
        if (!v32_extract_steps(&e->list.items[i], before, after)) return 0;
    return 1;
}

static int v32_is_compound_assign(const AstNode *n) {
    return n != NULL && n->kind == AST_ASSIGN && n->str1 != NULL && strcmp(n->str1, "=") != 0;
}

/* ---- writing through a pointer a call returned ------------------------------
 *
 * A second, related Vircon32 C fault (v26.04.24, found 20261009 while
 * adding operator++): when an assignment or ++/-- writes through a pointer
 * that a CALL produced, the call runs more than once --
 *
 *     *get() = 5;          get() runs twice
 *     get()->m = 7;        twice
 *     get()->m += 1;       three times
 *     (*get())++;          twice
 *
 * (plain reads, `x = *get();` and `x = get()->m;`, run it once; subscripts
 * and `*p++ = v` are fine). C++ produces this shape constantly: every
 * call of a reference-returning function or operator used as a target
 * (`v[i] = 5` through a user `T &operator[]`, `++it` returning `*this`,
 * `obj.ref() = x`) is `*call(...)` in the generated C. So when such a
 * statement's target writes through a call's result, the pointer is
 * computed once into a temporary:
 *
 *     get()->m = 7;   ->   { S *__v32_ptr0 = get(); __v32_ptr0->m = 7; }
 *
 * (The read-only `(*get()).m` case is fixed in codegen, which prints it as
 * `(get())->m`.) */
static int v32_has_call(const AstNode *e) {
    if (e == NULL) return 0;
    if (e->kind == AST_CALL) return 1;
    if (v32_has_call(e->a) || v32_has_call(e->b) || v32_has_call(e->c)) return 1;
    for (int i = 0; i < e->list.count; i++)
        if (v32_has_call(e->list.items[i])) return 1;
    return 0;
}

/* Within an lvalue, the outermost pointer operand (of a `*` or `->`)
 * that has a call in it, or NULL. Subscript indexes and the like are
 * evaluated once already and are not looked into. */
static AstNode **v32_called_pointer(AstNode **slot) {
    while (slot != NULL && *slot != NULL) {
        AstNode *e = *slot;
        if (e->kind == AST_MEMBER) {
            if (e->str1 != NULL && strcmp(e->str1, "->") == 0 && v32_has_call(e->a)) return &e->a;
            slot = &e->a;
        } else if (e->kind == AST_UNOP && e->str1 != NULL && strcmp(e->str1, "deref") == 0) {
            if (v32_has_call(e->a)) return &e->a;
            slot = &e->a;
        } else if (e->kind == AST_SUBSCRIPT) {
            slot = &e->a;
        } else {
            return NULL;
        }
    }
    return NULL;
}

/* An expression statement `target = value;`, `target++;` (any of the four
 * steps) or `target op= value;` writing through a called pointer: see
 * above. Returns 1 if *slot became a block. */
static int v32_hoist_called_pointer(AstNode **slot, AstNode *cls, LocalVarType *locals) {
    AstNode *stmt = *slot;
    AstNode *n = stmt->a;
    if (n == NULL) return 0;
    AstNode **target;
    if (n->kind == AST_ASSIGN) target = &n->a;
    else if (v32_is_step(n)) target = &n->a;
    else return 0;
    AstNode **ptr_slot = v32_called_pointer(target);
    if (ptr_slot == NULL) return 0;
    const AstNode *type = infer_expr_type(*ptr_slot, cls, locals);
    const AstNode *resolved = type ? resolve_typedef_chain(type) : NULL;
    while (resolved != NULL && resolved->kind == AST_CONST_TYPE) resolved = resolve_typedef_chain(resolved->a);
    if (resolved == NULL || resolved->kind != AST_POINTER_TYPE) {
        fprintf(stderr, "%s:%d: warning: this statement writes through a pointer returned by a "
                "call, and Vircon32 C evaluates that call more than once; store the pointer "
                "in a variable first\n",
                stmt->file ? stmt->file : g_current_filename, stmt->line);
        return 0;
    }
    static int counter = 0;
    char name[40];
    snprintf(name, sizeof name, "__v32_ptr%d", counter++);
    AstNode *decl = ast_new(AST_VAR_DECL, stmt->line);
    decl->str1 = strdup(name);
    decl->type = (AstNode *)type;
    decl->a = *ptr_slot;
    *ptr_slot = ast_ident(name, stmt->line);
    AstNode *block = ast_new(AST_BLOCK, stmt->line);
    ast_list_append(&block->list, decl);
    ast_list_append(&block->list, stmt);
    *slot = block;
    lower_note(stmt->line, "stored a call's pointer result before writing through it -- "
        "Vircon32 C runs a call in an assignment target more than once");
    return 1;
}

/* An expression statement `target op= value;`: see above. */
static void v32_split_compound_stmt(AstNode **slot, AstNode *cls, LocalVarType *locals) {
    AstNode *stmt = *slot;
    AstNode *n = stmt->a;
    if (v32_hoist_called_pointer(slot, cls, locals)) return;
    if (!v32_is_compound_assign(n) || !v32_has_side_effect(n->a)) return;

    /* The general way, whenever the target's type is known: take its
     * address once. */
    const AstNode *type = infer_expr_type(n->a, cls, locals);
    const AstNode *resolved = type ? resolve_typedef_chain(type) : NULL;
    if (resolved != NULL && resolved->kind != AST_ARRAY_TYPE && resolved->kind != AST_CONST_TYPE &&
        resolved->kind != AST_REFERENCE_TYPE) {
        static int counter = 0;
        char name[40];
        snprintf(name, sizeof name, "__v32_target%d", counter++);
        AstNode *addr = ast_new(AST_UNOP, stmt->line);
        addr->str1 = strdup("addr");
        addr->a = n->a;
        AstNode *ptr = ast_new(AST_VAR_DECL, stmt->line);
        ptr->str1 = strdup(name);
        ptr->type = ast_wrap_pointer((AstNode *)type, stmt->line);
        ptr->a = addr;
        AstNode *deref = ast_new(AST_UNOP, stmt->line);
        deref->str1 = strdup("deref");
        deref->a = ast_ident(name, stmt->line);
        n->a = deref;
        AstNode *block = ast_new(AST_BLOCK, stmt->line);
        ast_list_append(&block->list, ptr);
        ast_list_append(&block->list, stmt);
        *slot = block;
        lower_note(stmt->line, "took the address of a compound assignment's target once -- "
            "Vircon32 C evaluates that target twice, side effects included");
        return;
    }
    AstList before = ast_list_new(), after = ast_list_new();
    AstNode *target = ast_clone_expr(n->a);
    if (!v32_extract_steps(&target, &before, &after)) return;    /* warned about in v32_compat_expr */
    n->a = target;
    AstNode *block = ast_new(AST_BLOCK, stmt->line);
    block->list = before;
    ast_list_append(&block->list, stmt);
    for (int i = 0; i < after.count; i++) ast_list_append(&block->list, after.items[i]);
    *slot = block;
    lower_note(stmt->line, "moved ++/-- out of the target of a compound assignment -- "
        "Vircon32 C evaluates that target twice");
}

static void v32_compat_expr(AstNode **slot, AstNode *cls, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    if (v32_is_compound_assign(n) && v32_has_side_effect(n->a)) {
        fprintf(stderr, "%s:%d: warning: the target of this `%s` has a side effect, and "
                "Vircon32 C evaluates the target of a compound assignment twice -- "
                "the side effect will happen twice; write it as separate statements\n",
                n->file ? n->file : g_current_filename, n->line, n->str1);
    }
    switch (n->kind) {
        case AST_BINOP:
            v32_compat_expr(&n->a, cls, locals);
            v32_compat_expr(&n->b, cls, locals);
            if (v32_op_decays(n->str1)) {
                v32_decay_array(&n->a, cls, locals);
                v32_decay_array(&n->b, cls, locals);
            }
            break;
        case AST_ASSIGN:
            v32_compat_expr(&n->a, cls, locals);
            v32_compat_expr(&n->b, cls, locals);
            if (n->str1 != NULL && strcmp(n->str1, "=") == 0) {
                v32_unconst_read(&n->b, cls, locals);
            }
            break;
        case AST_SUBSCRIPT:
            v32_compat_expr(&n->a, cls, locals);
            v32_compat_expr(&n->b, cls, locals);
            break;
        case AST_UNOP:
            v32_compat_expr(&n->a, cls, locals);
            if (n->str1 != NULL && strcmp(n->str1, "deref") == 0) {
                v32_decay_array(&n->a, cls, locals);
            }
            break;
        case AST_CAST:
            v32_compat_expr(&n->a, cls, locals);
            v32_decay_array(&n->a, cls, locals);
            break;
        case AST_MEMBER:
            v32_compat_expr(&n->a, cls, locals);
            break;
        case AST_CALL:
            v32_compat_expr(&n->a, cls, locals);
            for (int i = 0; i < n->list.count; i++) {
                v32_compat_expr(&n->list.items[i], cls, locals);
                v32_unconst_read(&n->list.items[i], cls, locals);
            }
            {
                /* A pointer-to-const handed to a C function the program
                 * did not write (`print(name.c_str())`, `strlen(text)`
                 * with a `const char *text`): the Vircon32 SDK declares
                 * its text parameters as plain `int *`, and Vircon32 C
                 * rejects the const pointer outright. Cast the const
                 * away, as for every other const read. */
                CallResolution *ncr = (CallResolution *)n->sema_info;
                if (ncr == NULL || ncr->resolved_target == NULL) {
                    for (int i = 0; i < n->list.count; i++) {
                        AstNode *arg = n->list.items[i];
                        if (arg == NULL || arg->kind == AST_CAST || arg->kind == AST_STRING_LIT) continue;
                        const AstNode *at = v32_strip_const(infer_expr_type(arg, cls, locals));
                        if (at == NULL || at->kind != AST_POINTER_TYPE || at->a == NULL ||
                            at->a->kind != AST_CONST_TYPE || !v32_is_builtin_scalar(at->a->a)) continue;
                        AstNode *cast = ast_new(AST_CAST, arg->line);
                        cast->type = ast_wrap_pointer(at->a->a, arg->line);
                        cast->a = arg;
                        n->list.items[i] = cast;
                        lower_note(arg->line, "cast a pointer-to-const argument of a C function to a "
                            "plain pointer -- Vircon32 C rejects passing it (\"discards const qualifier\")");
                    }
                }
            }
            break;
        case AST_INIT_LIST:
        case AST_DIRECT_INIT:
            for (int i = 0; i < n->list.count; i++) {
                v32_compat_expr(&n->list.items[i], cls, locals);
            }
            break;
        default:
            /* literals, identifiers, and AST_SIZEOF -- whose operand is
             * never evaluated, and where `sizeof arr` must NOT decay */
            break;
    }
}

static void v32_compat_stmt(AstNode **slot, AstNode *cls, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK: {
            LocalVarType *outer = *locals; /* a block's locals end with it */
            for (int i = 0; i < n->list.count; i++) {
                v32_compat_stmt(&n->list.items[i], cls, locals);
            }
            *locals = outer;
            break;
        }
        case AST_IF:
            v32_compat_expr(&n->a, cls, *locals);
            v32_compat_stmt(&n->b, cls, locals);
            v32_compat_stmt(&n->c, cls, locals);
            break;
        case AST_WHILE:
            v32_compat_expr(&n->a, cls, *locals);
            v32_compat_stmt(&n->b, cls, locals);
            break;
        case AST_FOR: {
            LocalVarType *outer = *locals;
            v32_compat_stmt(&n->a, cls, locals);
            v32_compat_expr(&n->b, cls, *locals);
            v32_compat_expr(&n->c, cls, *locals);
            v32_compat_stmt(&n->d, cls, locals);
            *locals = outer;
            break;
        }
        case AST_SWITCH:
            v32_compat_expr(&n->a, cls, *locals);
            for (int i = 0; i < n->list.count; i++) {
                v32_compat_stmt(&n->list.items[i], cls, locals);
            }
            break;
        case AST_LABEL:
            v32_compat_stmt(&n->a, cls, locals);
            break;
        case AST_RETURN:
            v32_compat_expr(&n->a, cls, *locals);
            v32_unconst_read(&n->a, cls, *locals);
            break;
        case AST_EXPR_STMT:
            v32_split_compound_stmt(slot, cls, *locals);
            if (*slot != n) {
                v32_compat_stmt(slot, cls, locals);     /* now a block */
                break;
            }
            v32_compat_expr(&n->a, cls, *locals);
            break;
        case AST_VAR_DECL: {
            v32_compat_expr(&n->a, cls, *locals);
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

static void v32_compat_in_method(AstNode *method, AstNode *class_decl) {
    if (method->kind != AST_FUNC_DEF) return;
    LocalVarType *locals = seed_locals_from_params(method);
    v32_compat_stmt(&method->a, class_decl, &locals);
}

static void v32_compat_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    v32_compat_in_method(layout->methods.items[j], n);
                }
            }
        } else if (n->kind == AST_NAMESPACE_DECL) {
            v32_compat_classes(&n->list);
        }
    }
}

static void v32_compat_free_functions(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) {
            v32_compat_free_functions(&n->list);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            v32_compat_in_method(n, NULL);
        }
    }
}

/* ---- phase 9a: destructor chaining ---------------------------------------
 *
 * A destructor's job doesn't end with its own body: C++ then destroys the
 * class's members, in reverse declaration order, and finally its base.
 * Nothing did that here -- `~Derived()` ran and `~Base()` never did, for
 * a local leaving scope, a `delete`, and an array alike, since all three
 * call just the one destructor. This appends the missing calls to every
 * destructor that has a body (implicit ones included -- see
 * synthesize_implicit_members in ast.c):
 *
 *     void Both__dtor(Both *this) {
 *         ...the body as written...
 *         Part__dtor(&this->part);        // members, last declared first
 *         Base__dtor((Base *)this);       // then the base
 *     }
 *
 * The base call is direct, never through the vtable: by the time a
 * derived destructor finishes, the object is "only a Base" (a virtual
 * destructor affects which destructor `delete` STARTS with, not this).
 * A `return;` inside the body must not skip the chain, so it becomes a
 * goto to a label placed just ahead of it.
 *
 * Runs after phase 9, which destroys the body's own locals at each
 * return, so those still happen first. By-value members only, matching
 * what constructor injection constructs, plus one-dimensional member
 * arrays of class objects (element by element, last first).
 */
static int stmt_has_return(const AstNode *n) {
    if (n == NULL) return 0;
    if (n->kind == AST_RETURN) return 1;
    if (stmt_has_return(n->a) || stmt_has_return(n->b) ||
        stmt_has_return(n->c) || stmt_has_return(n->d)) return 1;
    for (int i = 0; i < n->list.count; i++)
        if (stmt_has_return(n->list.items[i])) return 1;
    return 0;
}

static void returns_to_goto(AstNode **slot, const char *label) {
    AstNode *n = *slot;
    if (n == NULL) return;
    if (n->kind == AST_RETURN) {
        AstNode *g = ast_new(AST_GOTO, n->line);
        g->str1 = strdup(label);
        *slot = g;
        return;
    }
    switch (n->kind) {
        case AST_BLOCK:
        case AST_SWITCH:
            for (int i = 0; i < n->list.count; i++) returns_to_goto(&n->list.items[i], label);
            break;
        case AST_IF:    returns_to_goto(&n->b, label); returns_to_goto(&n->c, label); break;
        case AST_WHILE: returns_to_goto(&n->b, label); break;
        case AST_FOR:   returns_to_goto(&n->d, label); break;
        case AST_LABEL: returns_to_goto(&n->a, label); break;
        default: break;
    }
}

static AstNode *dtor_call_on(AstNode *dtor, AstNode *object_ptr, int line) {
    FuncSemaInfo *info = (FuncSemaInfo *)dtor->sema_info;
    AstNode *call = ast_new(AST_CALL, line);
    call->a = ast_ident(info != NULL ? info->mangled_name : dtor->str1, line);
    ast_list_append(&call->list, object_ptr);
    AstNode *st = ast_new(AST_EXPR_STMT, line);
    st->a = call;
    return st;
}

static void chain_destructors_classes(AstList *decls) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_NAMESPACE_DECL) { chain_destructors_classes(&n->list); continue; }
        if (n->kind != AST_CLASS_DECL) continue;
        ClassLayout *layout = (ClassLayout *)n->sema_info;
        if (layout == NULL) continue;
        AstNode *dtor = find_destructor_with_body(n);
        if (dtor == NULL || dtor->a == NULL || dtor->a->kind != AST_BLOCK) continue;
        int line = dtor->line;

        AstList chain = ast_list_new();
        for (int f = layout->data_members.count - 1; f >= 0; f--) {
            AstNode *field = layout->data_members.items[f];
            if (field->type == NULL || field->type->kind == AST_POINTER_TYPE ||
                field->type->kind == AST_REFERENCE_TYPE) continue;
            /* A member array of class objects is destroyed element by
             * element, last first -- the mirror of build_member_ctor_stmt. */
            AstNode *aclass = member_array_elem_class(field->type);
            if (aclass != NULL) {
                AstNode *adtor = (aclass != n) ? find_destructor_with_body(aclass) : NULL;
                if (adtor == NULL) continue;
                char idx[64];
                snprintf(idx, sizeof idx, "__v32_mdtor_i%d", g_member_array_loop_counter++);
                AstNode *eaddr = ast_new(AST_UNOP, line);
                eaddr->str1 = strdup("addr");
                eaddr->a = member_object_ref(field, idx, line);
                ast_list_append(&chain, member_array_loop(field->type, idx,
                                    dtor_call_on(adtor, eaddr, line), 1, line));
                continue;
            }
            if (field->type->kind == AST_ARRAY_TYPE) continue;
            AstNode *mclass = type_to_class(field->type);
            if (mclass == NULL || mclass == n) continue;
            AstNode *mdtor = find_destructor_with_body(mclass);
            if (mdtor == NULL) continue;
            AstNode *ref = ast_new(AST_MEMBER, line);
            ref->str1 = strdup("->");
            ref->str2 = strdup(field->str1);
            ref->a = ast_ident("this", line);
            AstNode *addr = ast_new(AST_UNOP, line);
            addr->str1 = strdup("addr");
            addr->a = ref;
            if (resolve_typedef_chain(field->type)->kind == AST_CONST_TYPE) {
                AstNode *cast = ast_new(AST_CAST, line);
                cast->type = ast_wrap_pointer(ast_ident(mclass->str1, line), line);
                cast->a = addr;
                addr = cast;
            }
            ast_list_append(&chain, dtor_call_on(mdtor, addr, line));
        }
        AstNode *base = layout->base_class_decl;
        AstNode *bdtor = (base != NULL) ? find_destructor_with_body(base) : NULL;
        if (bdtor != NULL) {
            AstNode *cast = ast_new(AST_CAST, line);
            cast->type = ast_wrap_pointer(ast_ident(base->str1, line), line);
            cast->a = ast_ident("this", line);
            ast_list_append(&chain, dtor_call_on(bdtor, cast, line));
        }
        if (chain.count == 0) continue;

        if (stmt_has_return(dtor->a)) {
            returns_to_goto(&dtor->a, "__v32_dtor_chain");
            AstNode *label = ast_new(AST_LABEL, line);
            label->str1 = strdup("__v32_dtor_chain");
            label->a = chain.items[0];
            chain.items[0] = label;
        }
        for (int k = 0; k < chain.count; k++) ast_list_append(&dtor->a->list, chain.items[k]);
        lower_note(line, "appended %d member/base destructor call(s) to %s",
                   chain.count, dtor->str1);
    }
}

/* ---- phase 9b: by-value structs across calls (--target=vircon32 only) ----
 *
 * Vircon32 C moves exactly one word per parameter and per return value:
 * "functions cannot return values of size > 1" / "functions cannot pass
 * arguments of size > 1". C and C++ have no such limit, so a struct (or
 * class) of more than one word that is returned or passed BY VALUE is
 * rewritten to travel by address, the way most native ABIs do it anyway:
 *
 *   Vec make( int x, int y )            void make( Vec *__v32_ret, int x, int y )
 *   { ... return v; }                   { ... *__v32_ret = v; return; }
 *
 *   Vec add( Vec a, Vec b )             void add( Vec *__v32_ret,
 *   { ... }                                       const Vec *__v32_byval_a,
 *                                                 const Vec *__v32_byval_b )
 *                                       { Vec a = *__v32_byval_a;
 *                                         Vec b = *__v32_byval_b; ... }
 *
 *   Vec c = add( a, b );                Vec c;  add( &c, &a, &b );
 *   c = add( a, b );                    add( &c, &a, &b );
 *   n = make( 1, 2 ).x + 1;             n = ( make( &tmp, 1, 2 ), tmp ).x + 1;
 *   return make( x, y );                make( __v32_ret, x, y ); return;
 *
 * The hidden result pointer goes right after `this` in a method. The
 * callee copies each by-value parameter on entry, so it still owns a
 * private copy it may modify, and the caller passes plain addresses. A
 * result used inside a larger expression lands in a temporary declared at
 * the top of the function and is spliced in with a comma operator, which
 * phase 10 (run next) turns into statements with C's evaluation order
 * intact -- loop conditions and the right side of && / || included.
 *
 * Runs after constructor/destructor injection (phases 7 and 9), so the
 * temporaries are plain storage: nothing constructs or destructs them.
 * Mangled names come from sema and do not change. Function-pointer types
 * are rewritten the same way, so callbacks and vtable slots agree with
 * the functions stored in them.
 *
 * Two passes over the whole program, in this order:
 *   1. every function BODY (call sites and returns), while every
 *      signature still says what the source said -- that is how a call is
 *      recognised as returning a struct;
 *   2. every SIGNATURE and function-pointer type.
 *
 * Not covered: unions (no layout is computed for them here), and a
 * struct-returning call in a file-scope initializer, which no C allows.
 */
static int abi_type_words(const AstNode *type) {
    type = resolve_typedef_chain(type);
    while (type != NULL && type->kind == AST_CONST_TYPE) type = resolve_typedef_chain(type->a);
    if (type == NULL) return 1;
    if (type->kind == AST_ARRAY_TYPE) return type->ival * abi_type_words(type->a);
    if (type->kind != AST_IDENT && type->kind != AST_QUALIFIED_ID) return 1;
    AstNode *cls = type_to_class(type);
    if (cls == NULL) return 1;
    StructLayout *layout = (StructLayout *)cls->lower_info;
    if (layout == NULL) return 1;
    int words = 0;
    for (int i = 0; i < layout->count; i++) {
        words += (layout->fields[i].kind == FIELD_DATA_MEMBER)
               ? abi_type_words(layout->fields[i].type) : 1;
    }
    return words;
}

/* The class of a struct too big to travel by value, or NULL. */
static AstNode *abi_big_struct(const AstNode *type) {
    const AstNode *t = resolve_typedef_chain(type);
    while (t != NULL && t->kind == AST_CONST_TYPE) t = resolve_typedef_chain(t->a);
    if (t == NULL || (t->kind != AST_IDENT && t->kind != AST_QUALIFIED_ID)) return NULL;
    AstNode *cls = type_to_class(t);
    if (cls == NULL) return NULL;
    return abi_type_words(t) > 1 ? cls : NULL;
}

typedef struct AbiCtx {
    AstNode *cls;            /* enclosing class, or NULL */
    AstNode *func;           /* the function being rewritten */
    AstNode *ret_class;      /* non-NULL if `func` itself returns a big struct */
    AstList temps;           /* result temporaries to declare at the top */
    int next_tmp;
} AbiCtx;

static AstNode *abi_addr(AstNode *e) {
    AstNode *addr = ast_new(AST_UNOP, e->line);
    addr->str1 = strdup("addr");
    addr->a = e;
    return addr;
}

/* What a call returns and takes, from its resolved target or, for a call
 * through a function pointer, from the pointer's type. Returns 0 if
 * neither is known. `params` items are AST_PARAM nodes (target) or bare
 * types (function-pointer type); *has_this says whether params[0] is the
 * receiver. */
static int abi_call_signature(AstNode *call, AbiCtx *cx, LocalVarType *locals,
                              const AstNode **ret, const AstList **params,
                              int *params_are_types, int *has_this) {
    CallResolution *cr = (CallResolution *)call->sema_info;
    if (cr != NULL && cr->resolved_target != NULL) {
        AstNode *t = cr->resolved_target;
        *ret = t->type;
        *params = &t->list;
        *params_are_types = 0;
        *has_this = (t->list.count > 0 && t->list.items[0]->str1 != NULL &&
                     strcmp(t->list.items[0]->str1, "this") == 0);
        return 1;
    }
    const AstNode *ft = resolve_typedef_chain(infer_expr_type(call->a, cx->cls, locals));
    if (ft != NULL && ft->kind == AST_FUNC_PTR_TYPE) {
        *ret = ft->type;
        *params = &ft->list;
        *params_are_types = 1;
        *has_this = 0;
        return 1;
    }
    return 0;
}

static void abi_expr(AstNode **slot, AbiCtx *cx, LocalVarType **locals);

/* Rewrites the ARGUMENTS of `call` (nested calls first, then big structs
 * passed by value -> their address) and reports whether the call itself
 * returns a big struct. Does not touch how the result is used. */
static AstNode *abi_prepare_call(AstNode *call, AbiCtx *cx, LocalVarType **locals, int *hidden_index) {
    const AstNode *ret = NULL;
    const AstList *params = NULL;
    int are_types = 0, has_this = 0;
    int known = abi_call_signature(call, cx, *locals, &ret, &params, &are_types, &has_this);
    abi_expr(&call->a, cx, locals);
    /* a method call that phase 3 did not expand keeps its receiver in the
     * callee expression; the argument list then lines up with the
     * parameters after `this` */
    int offset = 0;
    if (known && params->count == call->list.count + 1 && has_this) offset = 1;
    for (int i = 0; i < call->list.count; i++) {
        abi_expr(&call->list.items[i], cx, locals);
        if (!known || i + offset >= params->count) continue;
        const AstNode *p = params->items[i + offset];
        const AstNode *ptype = are_types ? p : p->type;
        if (abi_big_struct(ptype) != NULL) {
            call->list.items[i] = abi_addr(call->list.items[i]);
        }
    }
    *hidden_index = (has_this && offset == 0) ? 1 : 0;
    if (*hidden_index > call->list.count) *hidden_index = call->list.count;
    return known ? abi_big_struct(ret) : NULL;
}

static void abi_insert_arg(AstNode *call, int index, AstNode *arg) {
    AstList out = ast_list_new();
    for (int i = 0; i < index; i++) ast_list_append(&out, call->list.items[i]);
    ast_list_append(&out, arg);
    for (int i = index; i < call->list.count; i++) ast_list_append(&out, call->list.items[i]);
    call->list = out;
}

static int abi_is_call(const AstNode *e) {
    return e != NULL && e->kind == AST_CALL && !e->v32_abi_done;
}

/* `call`, a not-yet-rewritten call: if it returns a big struct, make it
 * store its result through `dest` (a pointer expression) and return 1.
 * Otherwise only its arguments are rewritten. */
static int abi_call_into(AstNode *call, AstNode *dest, AbiCtx *cx, LocalVarType **locals) {
    int index = 0;
    AstNode *cls = abi_prepare_call(call, cx, locals, &index);
    call->v32_abi_done = 1;
    if (cls == NULL) return 0;
    abi_insert_arg(call, index, dest);
    return 1;
}

static void abi_expr(AstNode **slot, AbiCtx *cx, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_CALL: {
            if (n->v32_abi_done) return;
            int index = 0;
            AstNode *cls = abi_prepare_call(n, cx, locals, &index);
            n->v32_abi_done = 1;
            if (cls == NULL) return;
            /* result needed as a value: ( call( &tmp, ... ), tmp ) */
            char name[64];
            snprintf(name, sizeof name, "__v32_sret_tmp%d", cx->next_tmp++);
            AstNode *decl = ast_new(AST_VAR_DECL, n->line);
            decl->str1 = strdup(name);
            decl->type = ast_ident(cls->str1, n->line);
            ast_list_append(&cx->temps, decl);
            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = decl->str1;
            lv->type = decl->type;
            lv->next = *locals;
            *locals = lv;
            abi_insert_arg(n, index, abi_addr(ast_ident(name, n->line)));
            AstNode *comma = ast_new(AST_BINOP, n->line);
            comma->str1 = strdup(",");
            comma->a = n;
            comma->b = ast_ident(name, n->line);
            *slot = comma;
            lower_note(n->line, "struct '%s' returned by value: the call writes "
                "into %s through a hidden pointer (Vircon32 C returns one word)",
                cls->str1, name);
            return;
        }
        case AST_SIZEOF:
            return; /* unevaluated */
        case AST_BINOP:
        case AST_ASSIGN:
        case AST_SUBSCRIPT:
            abi_expr(&n->a, cx, locals);
            abi_expr(&n->b, cx, locals);
            return;
        case AST_TERNARY:
            abi_expr(&n->a, cx, locals);
            abi_expr(&n->b, cx, locals);
            abi_expr(&n->c, cx, locals);
            return;
        case AST_UNOP:
        case AST_CAST:
            abi_expr(&n->a, cx, locals);
            return;
        case AST_MEMBER: {
            abi_expr(&n->a, cx, locals);
            /* `one( 42 ).v` -- a ONE-word struct is returned natively, but
             * Vircon32 C cannot select a member of a call's result
             * ("cannot emit memory placement when an expression has
             * none"): park it in a temporary first. */
            AstNode *call = n->a;
            if (call == NULL || call->kind != AST_CALL) return;
            if (n->str1 != NULL && strcmp(n->str1, ".") != 0) return;
            const AstNode *rt = resolve_typedef_chain(infer_expr_type(call, cx->cls, *locals));
            while (rt != NULL && rt->kind == AST_CONST_TYPE) rt = resolve_typedef_chain(rt->a);
            if (rt == NULL || rt->kind == AST_POINTER_TYPE || type_to_class(rt) == NULL) return;
            AstNode *cls = type_to_class(rt);
            char name[64];
            snprintf(name, sizeof name, "__v32_sret_tmp%d", cx->next_tmp++);
            AstNode *decl = ast_new(AST_VAR_DECL, n->line);
            decl->str1 = strdup(name);
            decl->type = ast_ident(cls->str1, n->line);
            ast_list_append(&cx->temps, decl);
            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = decl->str1;
            lv->type = decl->type;
            lv->next = *locals;
            *locals = lv;
            AstNode *assign = ast_new(AST_ASSIGN, n->line);
            assign->str1 = strdup("=");
            assign->a = ast_ident(name, n->line);
            assign->b = call;
            AstNode *comma = ast_new(AST_BINOP, n->line);
            comma->str1 = strdup(",");
            comma->a = assign;
            comma->b = ast_ident(name, n->line);
            n->a = comma;
            return;
        }
        case AST_INIT_LIST:
        case AST_DIRECT_INIT:
            for (int i = 0; i < n->list.count; i++) abi_expr(&n->list.items[i], cx, locals);
            return;
        default:
            return;
    }
}

static AstNode *abi_expr_stmt(AstNode *e) {
    AstNode *st = ast_new(AST_EXPR_STMT, e->line);
    st->a = e;
    return st;
}

static void abi_stmt(AstNode **slot, AbiCtx *cx, LocalVarType **locals);

static void abi_stmt_list(AstList *list, AbiCtx *cx, LocalVarType **locals) {
    AstList out = ast_list_new();
    for (int i = 0; i < list->count; i++) {
        AstNode *st = list->items[i];
        if (st != NULL && st->kind == AST_VAR_DECL && abi_is_call(st->a) &&
            st->type != NULL && st->type->kind != AST_ARRAY_TYPE) {
            /* `Vec c = add( a, b );` -> `Vec c; add( &c, ... );` */
            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = st->str1;
            lv->type = st->type;
            AstNode *dest = abi_addr(ast_ident(st->str1, st->line));
            AstNode *cls = abi_big_struct(st->type);
            if (cls != NULL && resolve_typedef_chain(st->type)->kind == AST_CONST_TYPE) {
                /* a const local still has to be written once */
                AstNode *cast = ast_new(AST_CAST, st->line);
                cast->type = ast_wrap_pointer(ast_ident(cls->str1, st->line), st->line);
                cast->a = dest;
                dest = cast;
            }
            AstNode *call = st->a;
            /* the variable is not in scope inside its own initializer */
            if (cls != NULL && abi_call_into(call, dest, cx, locals)) {
                st->a = NULL;
                ast_list_append(&out, st);
                ast_list_append(&out, abi_expr_stmt(call));
            } else {
                ast_list_append(&out, st);
            }
            lv->next = *locals;
            *locals = lv;
            continue;
        }
        if (cx->ret_class != NULL && st != NULL && st->kind == AST_VAR_DECL &&
            st->a != NULL && strncmp(st->str1, "__v32_ret_tmp", 13) == 0 &&
            i + 1 < list->count && list->items[i + 1] != NULL &&
            list->items[i + 1]->kind == AST_RETURN && list->items[i + 1]->a != NULL &&
            list->items[i + 1]->a->kind == AST_IDENT &&
            strcmp(list->items[i + 1]->a->str1, st->str1) == 0) {
            /* phase 9's `T tmp = E; return tmp;` with nothing to destroy
             * in between: return E directly and save a struct copy */
            list->items[i + 1]->a = st->a;
            continue;
        }
        abi_stmt(&list->items[i], cx, locals);
        ast_list_append(&out, list->items[i]);
    }
    *list = out;
}

static void abi_stmt(AstNode **slot, AbiCtx *cx, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK: {
            LocalVarType *outer = *locals;
            abi_stmt_list(&n->list, cx, locals);
            *locals = outer;
            break;
        }
        case AST_SWITCH:
            abi_expr(&n->a, cx, locals);
            abi_stmt_list(&n->list, cx, locals);
            break;
        case AST_IF:
            abi_expr(&n->a, cx, locals);
            abi_stmt(&n->b, cx, locals);
            abi_stmt(&n->c, cx, locals);
            break;
        case AST_WHILE:
            abi_expr(&n->a, cx, locals);
            abi_stmt(&n->b, cx, locals);
            break;
        case AST_FOR: {
            LocalVarType *outer = *locals;
            abi_stmt(&n->a, cx, locals);
            abi_expr(&n->b, cx, locals);
            abi_expr(&n->c, cx, locals);
            abi_stmt(&n->d, cx, locals);
            *locals = outer;
            break;
        }
        case AST_LABEL:
            abi_stmt(&n->a, cx, locals);
            break;
        case AST_VAR_DECL: {
            abi_expr(&n->a, cx, locals);
            LocalVarType *lv = calloc(1, sizeof(LocalVarType));
            lv->name = n->str1;
            lv->type = n->type;
            lv->next = *locals;
            *locals = lv;
            break;
        }
        case AST_EXPR_STMT: {
            AstNode *e = n->a;
            if (e != NULL && e->kind == AST_ASSIGN && e->str1 != NULL &&
                strcmp(e->str1, "=") == 0 && abi_is_call(e->b)) {
                /* `c = add( a, b );` -> `add( &c, ... );` */
                abi_expr(&e->a, cx, locals);
                AstNode *call = e->b;
                if (abi_call_into(call, abi_addr(e->a), cx, locals)) n->a = call;
                break;
            }
            abi_expr(&n->a, cx, locals);
            /* a struct result nobody uses: keep the call, drop the read */
            if (n->a != NULL && n->a->kind == AST_BINOP && n->a->str1 != NULL &&
                strcmp(n->a->str1, ",") == 0 && n->a->a != NULL &&
                n->a->a->kind == AST_CALL && n->a->a->v32_abi_done &&
                n->a->b != NULL && n->a->b->kind == AST_IDENT &&
                strncmp(n->a->b->str1, "__v32_sret_tmp", 14) == 0) {
                n->a = n->a->a;
            }
            break;
        }
        case AST_RETURN: {
            if (cx->ret_class == NULL || n->a == NULL) {
                abi_expr(&n->a, cx, locals);
                break;
            }
            /* `return E;` in a struct-returning function */
            AstNode *block = ast_new(AST_BLOCK, n->line);
            AstNode *ret_ptr = ast_ident("__v32_ret", n->line);
            AstNode *value = n->a;
            if (abi_is_call(value) && abi_call_into(value, ret_ptr, cx, locals)) {
                ast_list_append(&block->list, abi_expr_stmt(value));
            } else {
                abi_expr(&value, cx, locals);
                AstNode *deref = ast_new(AST_UNOP, n->line);
                deref->str1 = strdup("deref");
                deref->a = ret_ptr;
                AstNode *assign = ast_new(AST_ASSIGN, n->line);
                assign->str1 = strdup("=");
                assign->a = deref;
                assign->b = value;
                ast_list_append(&block->list, abi_expr_stmt(assign));
            }
            n->a = NULL;
            ast_list_append(&block->list, n);
            *slot = block;
            break;
        }
        default:
            break;
    }
}

/* Pass 1 for one function: call sites and returns in its body. */
static void abi_rewrite_body(AstNode *func, AstNode *class_decl) {
    if (func->kind != AST_FUNC_DEF || func->a == NULL || func->v32_abi_done) return;
    func->v32_abi_done = 1;
    AbiCtx cx;
    cx.cls = class_decl;
    cx.func = func;
    cx.ret_class = abi_big_struct(func->type);
    cx.temps = ast_list_new();
    cx.next_tmp = 0;
    LocalVarType *locals = seed_locals_from_params(func);
    /* __v32_ret is only a parameter after pass 2; phase 10/11 will see it
     * as one. Here nothing needs its type. */
    abi_stmt(&func->a, &cx, &locals);
    if (cx.temps.count > 0 && func->a->kind == AST_BLOCK) {
        AstList out = ast_list_new();
        for (int i = 0; i < cx.temps.count; i++) ast_list_append(&out, cx.temps.items[i]);
        for (int i = 0; i < func->a->list.count; i++) ast_list_append(&out, func->a->list.items[i]);
        func->a->list = out;
    }
}

/* Pass 2 helpers. */
static void abi_rewrite_types_in(AstNode *type);

static void abi_rewrite_func_ptr_type(AstNode *ft) {
    if (ft->v32_abi_done) return;
    ft->v32_abi_done = 1;
    AstList out = ast_list_new();
    AstNode *ret_cls = abi_big_struct(ft->type);
    if (ret_cls != NULL) {
        ast_list_append(&out, ast_wrap_pointer(ast_ident(ret_cls->str1, ft->line), ft->line));
        ft->type = ast_ident("void", ft->line);
    } else {
        abi_rewrite_types_in(ft->type);
    }
    for (int i = 0; i < ft->list.count; i++) {
        AstNode *p = ft->list.items[i];
        AstNode *cls = abi_big_struct(p);
        if (cls != NULL) {
            p = ast_wrap_pointer(ast_wrap_const(ast_ident(cls->str1, ft->line), ft->line), ft->line);
        } else {
            abi_rewrite_types_in(p);
        }
        ast_list_append(&out, p);
    }
    ft->list = out;
}

/* Finds function-pointer types inside `type` (behind pointers, arrays,
 * const) and rewrites them. Never follows a typedef NAME: the typedef's
 * own declaration is visited on its own. */
static void abi_rewrite_types_in(AstNode *type) {
    while (type != NULL) {
        if (type->kind == AST_FUNC_PTR_TYPE) { abi_rewrite_func_ptr_type(type); return; }
        if (type->kind == AST_POINTER_TYPE || type->kind == AST_REFERENCE_TYPE ||
            type->kind == AST_CONST_TYPE || type->kind == AST_ARRAY_TYPE) {
            type = type->a;
        } else {
            return;
        }
    }
}

static void abi_rewrite_local_types(AstNode *n) {
    if (n == NULL) return;
    switch (n->kind) {
        case AST_VAR_DECL: abi_rewrite_types_in(n->type); break;
        case AST_BLOCK:
        case AST_SWITCH:
            for (int i = 0; i < n->list.count; i++) abi_rewrite_local_types(n->list.items[i]);
            break;
        case AST_IF:    abi_rewrite_local_types(n->b); abi_rewrite_local_types(n->c); break;
        case AST_WHILE: abi_rewrite_local_types(n->b); break;
        case AST_FOR:   abi_rewrite_local_types(n->a); abi_rewrite_local_types(n->d); break;
        case AST_LABEL: abi_rewrite_local_types(n->a); break;
        default: break;
    }
}

/* Pass 2 for one function declaration or definition: its signature. */
static void abi_rewrite_signature(AstNode *func) {
    if ((func->kind != AST_FUNC_DECL && func->kind != AST_FUNC_DEF) || func->v32_abi_done == 2) return;
    func->v32_abi_done = 2;
    int line = func->line;
    AstList params = ast_list_new();
    AstList copies = ast_list_new();
    AstNode *ret_cls = abi_big_struct(func->type);
    int has_this = (func->list.count > 0 && func->list.items[0]->str1 != NULL &&
                    strcmp(func->list.items[0]->str1, "this") == 0);
    for (int i = 0; i < func->list.count; i++) {
        AstNode *p = func->list.items[i];
        if (i == (has_this ? 1 : 0) && ret_cls != NULL) {
            AstNode *rp = ast_new(AST_PARAM, line);
            rp->str1 = strdup("__v32_ret");
            rp->type = ast_wrap_pointer(ast_ident(ret_cls->str1, line), line);
            ast_list_append(&params, rp);
        }
        AstNode *cls = abi_big_struct(p->type);
        if (cls != NULL) {
            const char *name = p->str1 != NULL ? p->str1 : "arg";
            char hidden[256];
            snprintf(hidden, sizeof hidden, "__v32_byval_%s", name);
            if (func->kind == AST_FUNC_DEF && func->a != NULL && p->str1 != NULL) {
                /* `Vec a = *__v32_byval_a;` -- the callee's own copy */
                AstNode *deref = ast_new(AST_UNOP, line);
                deref->str1 = strdup("deref");
                deref->a = ast_ident(hidden, line);
                AstNode *copy = ast_new(AST_VAR_DECL, p->line);
                copy->str1 = p->str1;
                copy->type = p->type;
                copy->a = deref;
                ast_list_append(&copies, copy);
            }
            p->str1 = strdup(hidden);
            p->type = ast_wrap_pointer(ast_wrap_const(ast_ident(cls->str1, line), line), line);
            p->a = NULL; /* a by-value struct default argument was already
                            filled in at each call site */
        } else {
            abi_rewrite_types_in(p->type);
        }
        ast_list_append(&params, p);
    }
    if (ret_cls != NULL && func->list.count <= (has_this ? 1 : 0)) {
        AstNode *rp = ast_new(AST_PARAM, line);
        rp->str1 = strdup("__v32_ret");
        rp->type = ast_wrap_pointer(ast_ident(ret_cls->str1, line), line);
        ast_list_append(&params, rp);
    }
    func->list = params;
    if (ret_cls != NULL) {
        func->type = ast_ident("void", line);
    } else {
        abi_rewrite_types_in(func->type);
    }
    if (func->kind == AST_FUNC_DEF && func->a != NULL && func->a->kind == AST_BLOCK) {
        if (copies.count > 0) {
            for (int i = 0; i < func->a->list.count; i++) ast_list_append(&copies, func->a->list.items[i]);
            func->a->list = copies;
        }
        abi_rewrite_local_types(func->a);
    }
}

static void abi_walk(AstList *decls, int pass) {
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n == NULL) continue;
        if (n->kind == AST_NAMESPACE_DECL) {
            abi_walk(&n->list, pass);
        } else if (n->kind == AST_CLASS_DECL) {
            ClassLayout *layout = (ClassLayout *)n->sema_info;
            if (layout != NULL) {
                for (int j = 0; j < layout->methods.count; j++) {
                    AstNode *m = layout->methods.items[j];
                    if (pass == 1) abi_rewrite_body(m, n); else abi_rewrite_signature(m);
                }
            }
            for (int j = 0; j < n->list.count; j++) {
                AstNode *m = n->list.items[j];
                if (m == NULL) continue;
                if (pass == 2 && (m->kind == AST_FUNC_DECL || m->kind == AST_FUNC_DEF)) abi_rewrite_signature(m);
                if (pass == 2 && m->kind == AST_VAR_DECL) abi_rewrite_types_in(m->type);
            }
        } else if (n->kind == AST_FUNC_DEF || n->kind == AST_FUNC_DECL) {
            if (pass == 1) {
                if (n->kind == AST_FUNC_DEF && n->b == NULL) abi_rewrite_body(n, NULL);
            } else {
                abi_rewrite_signature(n);
            }
        } else if (pass == 2 && (n->kind == AST_VAR_DECL || n->kind == AST_TYPEDEF_DECL)) {
            abi_rewrite_types_in(n->type);
        } else if (pass == 2 && n->kind == AST_UNION_DECL) {
            for (int j = 0; j < n->list.count; j++)
                if (n->list.items[j] != NULL) abi_rewrite_types_in(n->list.items[j]->type);
        }
    }
}

/* The file-scope declarations, for looking a union up by name (phase 12). */
static const AstList *g_lower_unions = NULL;

static const AstNode *lower_find_union(const char *name) {
    if (g_lower_unions == NULL || name == NULL) return NULL;
    for (int i = 0; i < g_lower_unions->count; i++) {
        const AstNode *n = g_lower_unions->items[i];
        if (n->kind == AST_UNION_DECL && strcmp(n->str1, name) == 0) return n;
    }
    return NULL;
}

/* ---- phase 12: C input ------------------------------------------------------
 *
 * Runs only when the file being transpiled is C (g_c_mode), last of all,
 * and writes down the places where C's rules and Vircon32 C's differ --
 * each found by compiling a real C program (demos/c/rogue) with the real
 * compiler. See docs/C_INPUT.md.
 *
 *  1. The null pointer. In C it is 0: `p = 0`, `if (p)`, `!p`, zeroed
 *     memory, an uninitialized static. Vircon32 C's NULL is -1, and it
 *     takes no int where a pointer goes, nor a pointer where a condition
 *     goes. For C input the null pointer stays 0 -- codegen.c prints it
 *     as `((void *)0)` -- and
 *       - `NULL` and a literal 0 in a pointer position are that null;
 *       - `p == NULL` / `p != NULL` become `((int)p) == 0`;
 *       - a pointer used as a truth value -- the condition of
 *         if/while/for, an operand of ! && || -- becomes `((int)p)`.
 *  2. A function's name used as a value (`start_daemon(doctor, ...)`, a
 *     table of handlers, `op->o_putfunc == put_bool`) gets its `&`
 *     everywhere, not only in the assignments phase 3 handles.
 *  3. A call through a K&R function pointer, `void (*d_func)();` called
 *     as `(*d_func)(arg)`: Vircon32 C checks the call against the
 *     pointer's (empty) parameter list, so the pointer is cast to the
 *     type the call implies: `((void(int)*)d_func)(arg)`.
 *  4. A string literal holding a control character (`"\033[2J"`,
 *     `"\b \b"`): Vircon32 C's strings have no such escapes. It becomes a
 *     file-scope char array, and the literal a pointer to it.
 *  5. An initializer list is matched to its type: a string that
 *     initializes a char array MEMBER becomes the characters, padded with
 *     zeros to the array's length; a 0 that initializes a pointer member
 *     becomes the null pointer.
 *  6. `(void) f();` is just `f();`.
 */
static AstList g_c_hoisted_strings;

static AstNode *c_int_cast(AstNode *e) {
    AstNode *cast = ast_new(AST_CAST, e->line);
    cast->type = ast_ident("int", e->line);
    cast->a = e;
    return cast;
}

static AstNode *c_int_lit(int value, int line) {
    AstNode *n = ast_new(AST_INT_LIT, line);
    n->ival = value;
    return n;
}

static const AstNode *c_resolved_type(const AstNode *t) {
    if (t == NULL) return NULL;
    t = resolve_typedef_chain(t);
    while (t != NULL && t->kind == AST_CONST_TYPE) t = resolve_typedef_chain(t->a);
    return t;
}

static int c_type_is_pointerish(const AstNode *t) {
    t = c_resolved_type(t);
    return t != NULL && (t->kind == AST_POINTER_TYPE || t->kind == AST_FUNC_PTR_TYPE ||
                         t->kind == AST_ARRAY_TYPE);
}

static int c_is_void_pointer(const AstNode *t) {
    t = c_resolved_type(t);
    if (t == NULL || t->kind != AST_POINTER_TYPE) return 0;
    t = c_resolved_type(t->a);
    return t != NULL && t->kind == AST_IDENT && strcmp(t->str1, "void") == 0;
}

static int c_is_null(const AstNode *e) {
    return e != NULL && e->kind == AST_NULL_LIT;
}

/* A value used as a condition: a pointer becomes `((int)value)`.
 * Comparisons and logical operators are already truth values. */
static void c_truth(AstNode **slot, AstNode *cls, LocalVarType *locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    if (n->kind == AST_BINOP || n->kind == AST_BOOL_LIT || n->kind == AST_INT_LIT ||
        n->kind == AST_CALL) {
        if (n->kind != AST_CALL) return;
    }
    if (n->kind == AST_UNOP && n->str1 != NULL && strcmp(n->str1, "!") == 0) return;
    if (c_is_null(n)) { *slot = c_int_lit(0, n->line); return; }
    const AstNode *t = c_resolved_type(infer_expr_type(n, cls, locals));
    if (t == NULL) return;
    if (c_type_is_pointerish(t)) *slot = c_int_cast(n);
}

/* Does this string literal hold a character Vircon32 C's strings cannot
 * spell? (Its escapes are \n \r \t \\ \' \" -- see print_vircon32_string
 * in codegen.c.) */
static int c_string_needs_array(const char *raw) {
    for (const char *p = raw; *p != '\0'; p++) {
        if ((unsigned char)*p < ' ' && *p != '\t') return 1;
        if (*p != '\\' || p[1] == '\0') continue;
        p++;
        if (strchr("nrt\\'\"", *p) == NULL) return 1;
    }
    return 0;
}

/* The characters of a string literal, one AST_INT_LIT each, terminator
 * included, zero-padded to `length` when that is longer. */
static AstNode *c_string_chars(const char *raw, int length, int line) {
    AstNode *list = ast_new(AST_INIT_LIST, line);
    for (const char *p = raw; *p != '\0'; ) {
        int ch = (unsigned char)*p++;
        if (ch == '\\' && *p != '\0') ch = ast_decode_escape(&p);
        ast_list_append(&list->list, c_int_lit(ch, line));
    }
    ast_list_append(&list->list, c_int_lit(0, line));
    while (list->list.count < length) ast_list_append(&list->list, c_int_lit(0, line));
    return list;
}

static AstNode *c_hoist_string(const AstNode *lit) {
    char name[40];
    int line = lit->line;
    snprintf(name, sizeof name, "__v32_str%d", g_c_hoisted_strings.count);
    AstNode *var = ast_new(AST_VAR_DECL, line);
    var->str1 = strdup(name);
    var->a = c_string_chars(lit->str1, 0, line);
    var->type = ast_wrap_array(ast_ident("char", line), var->a->list.count, line);
    ast_list_append(&g_c_hoisted_strings, var);
    lower_note(line, "moved a string literal with a control character into the char "
        "array %s -- Vircon32 C's string literals have no escape for it", name);
    /* &__v32_strN[0] */
    AstNode *element = ast_new(AST_SUBSCRIPT, line);
    element->a = ast_ident(name, line);
    element->b = c_int_lit(0, line);
    AstNode *addr = ast_new(AST_UNOP, line);
    addr->str1 = strdup("addr");
    addr->a = element;
    return addr;
}

/* The type a call through an unprototyped function pointer implies:
 * the pointer's return type, and one parameter per argument. */
static AstNode *c_unprototyped_call_type(const AstNode *fp, const AstNode *call,
                                         AstNode *cls, LocalVarType *locals) {
    AstList params = ast_list_new();
    for (int i = 0; i < call->list.count; i++) {
        const AstNode *at = c_resolved_type(infer_expr_type(call->list.items[i], cls, locals));
        AstNode *pt;
        if (at == NULL) pt = ast_ident("int", call->line);
        else if (at->kind == AST_ARRAY_TYPE) pt = ast_wrap_pointer(at->a, call->line);
        else pt = (AstNode *)at;
        ast_list_append(&params, pt);
    }
    return ast_wrap_func_ptr(fp->type, params, call->line);
}

/* A function (pointer) that takes parameters, stored where the program
 * declared a K&R pointer -- `void (*func)()`, parameters unspecified:
 * C allows it, Vircon32 C compares the two parameter lists. The value is
 * cast to the pointer's own type. */
static void c_unprototyped_target(AstNode **slot, const AstNode *target_type,
                                  AstNode *cls, LocalVarType *locals) {
    AstNode *e = *slot;
    const AstNode *tt = c_resolved_type(target_type);
    if (e == NULL || tt == NULL || tt->kind != AST_FUNC_PTR_TYPE || tt->list.count != 0) return;
    if (e->kind == AST_CAST || e->kind == AST_NULL_LIT) return;
    int params = 0;
    if (e->kind == AST_UNOP && e->str1 != NULL && strcmp(e->str1, "addr") == 0 &&
        e->a != NULL && e->a->kind == AST_IDENT && is_bare_free_function_ref(e->a, locals)) {
        AstNode **candidates = NULL;
        int count = 0, cap = 0;
        collect_free_function_candidates(e->a->str1, &candidates, &count, &cap);
        if (count > 0) params = candidates[0]->list.count;
        free(candidates);
    } else {
        const AstNode *st = c_resolved_type(infer_expr_type(e, cls, locals));
        if (st != NULL && st->kind == AST_FUNC_PTR_TYPE) params = st->list.count;
    }
    if (params == 0) return;
    AstNode *cast = ast_new(AST_CAST, e->line);
    cast->type = (AstNode *)target_type;
    cast->a = e;
    *slot = cast;
    lower_note(e->line, "cast a function with parameters to the parameterless (K&R) "
        "function pointer type it is stored in -- Vircon32 C compares the parameter lists");
}

static void c_expr(AstNode **slot, AstNode *cls, LocalVarType *locals, int is_callee) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_IDENT:
            if (n->str1 != NULL && strcmp(n->str1, "NULL") == 0 && find_local(locals, "NULL") == NULL) {
                *slot = ast_new(AST_NULL_LIT, n->line);
            } else if (!is_callee && is_bare_free_function_ref(n, locals)) {
                wrap_addr_of(slot);
            }
            break;
        case AST_STRING_LIT:
            if (c_string_needs_array(n->str1)) *slot = c_hoist_string(n);
            break;
        case AST_UNOP:
            if (n->str1 != NULL && strcmp(n->str1, "addr") == 0 && n->a != NULL &&
                n->a->kind == AST_IDENT) {
                break;                              /* `&name`: as written */
            }
            c_expr(&n->a, cls, locals, 0);
            if (n->str1 != NULL && strcmp(n->str1, "!") == 0) c_truth(&n->a, cls, locals);
            break;
        case AST_BINOP: {
            c_expr(&n->a, cls, locals, 0);
            c_expr(&n->b, cls, locals, 0);
            const char *op = n->str1 ? n->str1 : "";
            if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
                c_truth(&n->a, cls, locals);
                c_truth(&n->b, cls, locals);
            } else if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
                if (c_is_null(n->b) && !c_is_null(n->a)) {
                    n->a = c_int_cast(n->a);
                    n->b = c_int_lit(0, n->line);
                } else if (c_is_null(n->a) && !c_is_null(n->b)) {
                    n->b = c_int_cast(n->b);
                    n->a = c_int_lit(0, n->line);
                } else {
                    /* `thing == ptr` with a `void *ptr`: C compares any
                     * pointer with a void pointer; Vircon32 C wants the
                     * two types to match. Compare the addresses. */
                    int va = c_is_void_pointer(infer_expr_type(n->a, cls, locals));
                    int vb = c_is_void_pointer(infer_expr_type(n->b, cls, locals));
                    if (va != vb) {
                        n->a = c_int_cast(n->a);
                        n->b = c_int_cast(n->b);
                    }
                }
            }
            break;
        }
        case AST_ASSIGN:
            c_expr(&n->a, cls, locals, 0);
            c_expr(&n->b, cls, locals, 0);
            if (n->str1 != NULL && strcmp(n->str1, "=") == 0) {
                const AstNode *target = infer_expr_type(n->a, cls, locals);
                rewrite_zero_to_null(&n->b, target);
                c_unprototyped_target(&n->b, target, cls, locals);
            }
            break;
        case AST_TERNARY:
            c_expr(&n->a, cls, locals, 0);
            c_expr(&n->b, cls, locals, 0);
            c_expr(&n->c, cls, locals, 0);
            c_truth(&n->a, cls, locals);
            break;
        case AST_CALL: {
            c_expr(&n->a, cls, locals, 1);
            for (int i = 0; i < n->list.count; i++) c_expr(&n->list.items[i], cls, locals, 0);
            {
                CallResolution *cr = (CallResolution *)n->sema_info;
                if (cr != NULL && cr->resolved_target != NULL) {
                    const AstList *params = &cr->resolved_target->list;
                    for (int i = 0; i < n->list.count && i < params->count; i++)
                        c_unprototyped_target(&n->list.items[i], params->items[i]->type, cls, locals);
                }
            }
            /* through a function pointer declared with `()` */
            AstNode *fp_expr = n->a;
            if (fp_expr != NULL && fp_expr->kind == AST_UNOP && fp_expr->str1 != NULL &&
                strcmp(fp_expr->str1, "deref") == 0)
                fp_expr = fp_expr->a;
            if (fp_expr != NULL && n->list.count > 0 &&
                !(fp_expr->kind == AST_IDENT && is_bare_free_function_ref(fp_expr, locals))) {
                const AstNode *ft = c_resolved_type(infer_expr_type(fp_expr, cls, locals));
                if (ft != NULL && ft->kind == AST_FUNC_PTR_TYPE && ft->list.count == 0) {
                    AstNode *cast = ast_new(AST_CAST, n->line);
                    cast->type = c_unprototyped_call_type(ft, n, cls, locals);
                    cast->a = fp_expr;
                    n->a = cast;
                    lower_note(n->line, "cast a function pointer declared without parameters "
                        "to the type this call implies -- Vircon32 C checks the arguments "
                        "against the pointer's own (empty) parameter list");
                }
            }
            break;
        }
        case AST_CAST:
            c_expr(&n->a, cls, locals, 0);
            if (n->type != NULL && n->type->kind == AST_IDENT && strcmp(n->type->str1, "void") == 0)
                *slot = n->a;                       /* `(void) f();` */
            break;
        case AST_MEMBER:
            c_expr(&n->a, cls, locals, 0);
            break;
        case AST_SUBSCRIPT:
            c_expr(&n->a, cls, locals, 0);
            c_expr(&n->b, cls, locals, 0);
            break;
        case AST_INIT_LIST:
        case AST_DIRECT_INIT:
            for (int i = 0; i < n->list.count; i++) c_expr(&n->list.items[i], cls, locals, 0);
            break;
        default:
            break;      /* literals; AST_SIZEOF, whose operand is not evaluated */
    }
}

/* The members of a struct, in order (the first only, for a union). */
static void c_fix_init(const AstNode *type, AstNode **slot);

static void c_fix_init_members(const AstList *members, int first_only, AstNode *init) {
    int item = 0;
    for (int i = 0; i < members->count && item < init->list.count; i++) {
        const AstNode *m = members->items[i];
        if (m == NULL || m->kind != AST_VAR_DECL) continue;
        c_fix_init(m->type, &init->list.items[item++]);
        if (first_only) break;
    }
}

static void c_fix_init(const AstNode *type, AstNode **slot) {
    AstNode *init = *slot;
    const AstNode *t = c_resolved_type(type);
    if (init == NULL || t == NULL) return;
    if (init->kind == AST_INIT_LIST) {
        if (t->kind == AST_ARRAY_TYPE) {
            for (int i = 0; i < init->list.count; i++) c_fix_init(t->a, &init->list.items[i]);
        } else if (t->kind == AST_IDENT) {
            AstNode *cls = type_to_class(t);
            if (cls != NULL) {
                c_fix_init_members(&cls->list, 0, init);
            } else {
                const AstNode *u = lower_find_union(t->str1);
                if (u != NULL) c_fix_init_members(&u->list, 1, init);
            }
        }
        return;
    }
    if (init->kind == AST_STRING_LIT && t->kind == AST_ARRAY_TYPE) {
        const AstNode *elem = c_resolved_type(t->a);
        if (elem != NULL && elem->kind == AST_IDENT)
            *slot = c_string_chars(init->str1, t->ival, init->line);
        return;
    }
    rewrite_zero_to_null(slot, t);
    c_unprototyped_target(slot, type, NULL, NULL);
}

static void c_stmt(AstNode **slot, AstNode *cls, LocalVarType **locals) {
    AstNode *n = *slot;
    if (n == NULL) return;
    switch (n->kind) {
        case AST_BLOCK: {
            LocalVarType *outer = *locals;
            for (int i = 0; i < n->list.count; i++) c_stmt(&n->list.items[i], cls, locals);
            *locals = outer;
            break;
        }
        case AST_IF:
            c_expr(&n->a, cls, *locals, 0);
            c_truth(&n->a, cls, *locals);
            c_stmt(&n->b, cls, locals);
            c_stmt(&n->c, cls, locals);
            break;
        case AST_WHILE:
            c_expr(&n->a, cls, *locals, 0);
            c_truth(&n->a, cls, *locals);
            c_stmt(&n->b, cls, locals);
            break;
        case AST_FOR: {
            LocalVarType *outer = *locals;
            c_stmt(&n->a, cls, locals);
            c_expr(&n->b, cls, *locals, 0);
            c_truth(&n->b, cls, *locals);
            c_expr(&n->c, cls, *locals, 0);
            c_stmt(&n->d, cls, locals);
            *locals = outer;
            break;
        }
        case AST_SWITCH:
            c_expr(&n->a, cls, *locals, 0);
            for (int i = 0; i < n->list.count; i++) c_stmt(&n->list.items[i], cls, locals);
            break;
        case AST_LABEL:
            c_stmt(&n->a, cls, locals);
            break;
        case AST_RETURN:
        case AST_EXPR_STMT:
            c_expr(&n->a, cls, *locals, 0);
            break;
        case AST_VAR_DECL: {
            c_expr(&n->a, cls, *locals, 0);
            c_fix_init(n->type, &n->a);
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

static void c_input_fixups(AstNode *program) {
    AstList *decls = &program->list;
    g_lower_unions = decls;
    for (int i = 0; i < decls->count; i++) {
        AstNode *n = decls->items[i];
        if (n->kind == AST_VAR_DECL) {
            c_expr(&n->a, NULL, NULL, 0);
            c_fix_init(n->type, &n->a);
        } else if (n->kind == AST_FUNC_DEF && n->b == NULL) {
            LocalVarType *locals = seed_locals_from_params(n);
            c_stmt(&n->a, NULL, &locals);
        }
    }
    if (g_c_hoisted_strings.count > 0) {
        AstList out = g_c_hoisted_strings;
        for (int i = 0; i < decls->count; i++) ast_list_append(&out, decls->items[i]);
        *decls = out;
        g_c_hoisted_strings = ast_list_new();
    }
}

int lower_run(AstNode *program) {
    lower_notes_reset(); /* always start this run's log empty -- see
        lower_notes_print's own doc comment */
    compute_struct_layouts(&program->list);
    /* (The by-value word-size check that used to run here is gone: a
     * multi-word struct passed or returned by value is now rewritten by
     * phase 9b instead of warned about.) */
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
    relabel_reference_members(&program->list);      /* phase 5 (members first) */
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
    construct_globals_in_main(program);               /* phase 7b */
    destruct_scope_classes(&program->list);           /* phase 9 */
    destruct_scope_free_functions(&program->list);
    chain_destructors_classes(&program->list);        /* phase 9a */
    if (g_target == TARGET_VIRCON32) {
        /* phase 10 -- deliberately conditional, unlike every earlier
         * phase: standard C supports the ternary operator natively,
         * so standard-mode output keeps `cond ? a : b` exactly as
         * written. See this phase's own doc comment (just above) for
         * the full reasoning and the real, stated scope boundary on
         * which ternary-containing statements this actually rewrites. */
        abi_walk(&program->list, 1);                  /* phase 9b -- see its */
        abi_walk(&program->list, 2);                  /* own doc comment     */
        rewrite_ternary_classes(&program->list);
        rewrite_ternary_free_functions(&program->list);
        int errors = check_no_ternaries(program);
        if (errors > 0) return errors;
        v32_compat_classes(&program->list);           /* phase 11 */
        v32_compat_free_functions(&program->list);
        if (g_c_mode) c_input_fixups(program);        /* phase 12 */
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
